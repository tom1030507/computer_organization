/**
 * Copyright (c) 2024 Energy-Aware Cache Research
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors: Energy-Aware Cache Research Team
 */

#include "mem/cache/replacement_policies/energy_aware_rp.hh"

#include <cassert>
#include <memory>
#include <cmath>

#include "params/EnergyAwareRP.hh"
#include "base/random.hh"

EnergyAwareRP::EnergyAwareRP(const Params *p)
    : BaseReplacementPolicy(p),
      frequencyBits(p->frequency_bits),
      writeBits(p->write_bits),
      recencyWeight(p->recency_weight),
      frequencyWeight(p->frequency_weight),
      writeWeight(p->write_weight),
      dirtyWeight(p->dirty_weight),
      utilizationWeight(p->utilization_weight),
      pcmWriteCost(p->pcm_write_cost),
      pcmReadCost(p->pcm_read_cost),
      blockSize(p->block_size)
{
}

double
EnergyAwareRP::calculateEnergyCost(const std::shared_ptr<EnergyAwareReplData>& repl_data) const
{
    // Current time for recency calculation
    Tick currentTick = curTick();
    
    // 1. Recency factor (normalized by max possible age)
    double recencyFactor = 0.0;
    if (currentTick > repl_data->lastTouchTick) {
        // Normalize by current tick to get a value between 0 and 1
        recencyFactor = (double)(currentTick - repl_data->lastTouchTick) / (double)currentTick;
    }
      // 2. Frequency factor (inversely related to access frequency)
    // Higher frequency blocks have lower eviction cost
    double maxFreq = (1 << frequencyBits) - 1;
    double frequencyFactor = 1.0 - ((double)repl_data->accessFreq.read() / maxFreq);
    
    // 3. Write intensity factor (PCM write cost consideration)
    double maxWrites = (1 << writeBits) - 1;
    double writeIntensity = (double)repl_data->writeCount.read() / maxWrites;
    
    // 4. Dirty bit factor (write-back cost)
    double dirtyFactor = repl_data->isDirty ? 1.0 : 0.0;
    
    // 5. Utilization factor (spatial locality)
    double utilizationFactor = 1.0 - ((double)repl_data->bytesUsed / (double)blockSize);
      // 6. PCM energy cost calculation
    // Estimate future access cost based on pattern
    double futureReadCost = (double)repl_data->accessFreq.read() * pcmReadCost;
    double futureWriteCost = (double)repl_data->writeCount.read() * pcmWriteCost;
    double writeBackCost = repl_data->isDirty ? pcmWriteCost : 0.0;
    
    // Combined energy-aware cost function
    // Higher cost means better candidate for eviction
    double energyCost = 
        recencyWeight * recencyFactor +                    // Older blocks cost less to evict
        frequencyWeight * frequencyFactor +                // Less frequent blocks cost less to evict
        writeWeight * writeIntensity +                     // Write-intensive blocks cost more to keep
        dirtyWeight * dirtyFactor +                        // Dirty blocks cost more to evict (write-back)
        utilizationWeight * utilizationFactor +            // Underutilized blocks cost less to evict
        0.1 * (futureReadCost + futureWriteCost) -         // Future access cost (benefit of keeping)
        0.2 * writeBackCost;                               // Write-back cost (penalty of evicting)
    
    // Ensure cost is non-negative
    return std::max(0.0, energyCost);
}

void
EnergyAwareRP::invalidate(const std::shared_ptr<ReplacementData>& replacement_data) const
{
    // Reset all tracking information
    std::shared_ptr<EnergyAwareReplData> repl_data = 
        std::static_pointer_cast<EnergyAwareReplData>(replacement_data);
        
    repl_data->lastTouchTick = Tick(0);
    repl_data->accessFreq.reset();
    repl_data->writeCount.reset();
    repl_data->bytesUsed = 0;
    repl_data->isDirty = false;
    repl_data->predictedReuse = 0;
    repl_data->energyCost = 0.0;
}

void
EnergyAwareRP::touch(const std::shared_ptr<ReplacementData>& replacement_data) const
{    // Update access statistics
    std::shared_ptr<EnergyAwareReplData> repl_data = 
        std::static_pointer_cast<EnergyAwareReplData>(replacement_data);
        
    repl_data->lastTouchTick = curTick();
    repl_data->accessFreq.increment();
    
    // Recalculate energy cost
    repl_data->energyCost = calculateEnergyCost(repl_data);
}

void
EnergyAwareRP::reset(const std::shared_ptr<ReplacementData>& replacement_data) const
{    // Initialize for new entry
    std::shared_ptr<EnergyAwareReplData> repl_data = 
        std::static_pointer_cast<EnergyAwareReplData>(replacement_data);
        
    repl_data->lastTouchTick = curTick();
    repl_data->accessFreq.increment();  // First access
    repl_data->writeCount.reset();
    repl_data->bytesUsed = blockSize;  // Assume full utilization initially
    repl_data->isDirty = false;
    repl_data->predictedReuse = 1;  // Assume some reuse
    repl_data->energyCost = calculateEnergyCost(repl_data);
}

ReplaceableEntry*
EnergyAwareRP::getVictim(const ReplacementCandidates& candidates) const
{
    // There must be at least one replacement candidate
    assert(candidates.size() > 0);

    // Find the victim with the highest energy cost (best to evict)
    ReplaceableEntry* victim = candidates[0];
    double maxCost = std::static_pointer_cast<EnergyAwareReplData>(
        victim->replacementData)->energyCost;
    
    for (const auto& candidate : candidates) {
        std::shared_ptr<EnergyAwareReplData> repl_data = 
            std::static_pointer_cast<EnergyAwareReplData>(candidate->replacementData);
            
        // Recalculate cost to ensure it's up to date
        double cost = calculateEnergyCost(repl_data);
        repl_data->energyCost = cost;
        
        if (cost > maxCost) {
            victim = candidate;
            maxCost = cost;
        }
    }

    return victim;
}

std::shared_ptr<ReplacementData>
EnergyAwareRP::instantiateEntry()
{
    return std::shared_ptr<ReplacementData>(
        new EnergyAwareReplData(frequencyBits, writeBits));
}

void
EnergyAwareRP::updateWriteStats(const std::shared_ptr<ReplacementData>& replacement_data) const
{
    std::shared_ptr<EnergyAwareReplData> repl_data = 
        std::static_pointer_cast<EnergyAwareReplData>(replacement_data);
        
    repl_data->writeCount.increment();
    repl_data->energyCost = calculateEnergyCost(repl_data);
}

void
EnergyAwareRP::updateUtilization(const std::shared_ptr<ReplacementData>& replacement_data, 
                                uint32_t bytes_accessed) const
{
    std::shared_ptr<EnergyAwareReplData> repl_data = 
        std::static_pointer_cast<EnergyAwareReplData>(replacement_data);
        
    // Update utilization (track maximum bytes used)
    repl_data->bytesUsed = std::max(repl_data->bytesUsed, bytes_accessed);
    repl_data->energyCost = calculateEnergyCost(repl_data);
}

void
EnergyAwareRP::setDirtyStatus(const std::shared_ptr<ReplacementData>& replacement_data, 
                             bool is_dirty) const
{
    std::shared_ptr<EnergyAwareReplData> repl_data = 
        std::static_pointer_cast<EnergyAwareReplData>(replacement_data);
        
    repl_data->isDirty = is_dirty;
    repl_data->energyCost = calculateEnergyCost(repl_data);
}

EnergyAwareRP*
EnergyAwareRPParams::create()
{
    return new EnergyAwareRP(this);
}

