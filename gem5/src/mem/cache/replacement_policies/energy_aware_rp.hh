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

/**
 * @file
 * Declaration of an Energy-Aware replacement policy for PCM-based main memory.
 * This policy uses a cost function that considers:
 * 1. Recency (LRU-style temporal locality)
 * 2. Frequency (access count)
 * 3. Write intensity (PCM write energy cost)
 * 4. Dirty bit status (write-back cost)
 * 5. Block utilization (spatial locality)
 */

#ifndef __MEM_CACHE_REPLACEMENT_POLICIES_ENERGY_AWARE_RP_HH__
#define __MEM_CACHE_REPLACEMENT_POLICIES_ENERGY_AWARE_RP_HH__

#include "mem/cache/replacement_policies/base.hh"
#include "cpu/pred/sat_counter.hh"

struct EnergyAwareRPParams;

class EnergyAwareRP : public BaseReplacementPolicy
{
  protected:
    /** Energy-Aware specific implementation of replacement data. */
    struct EnergyAwareReplData : ReplacementData
    {
        /** Tick on which the entry was last touched. */
        Tick lastTouchTick;
        
        /** Access frequency counter (saturating counter). */
        SatCounter accessFreq;
        
        /** Write access counter for energy estimation. */
        SatCounter writeCount;
        
        /** Number of bytes actually used in this cache line. */
        uint32_t bytesUsed;
        
        /** Flag indicating if this block is dirty. */
        bool isDirty;
        
        /** Predicted reuse distance based on access pattern. */
        uint32_t predictedReuse;
        
        /** Energy cost estimation for this block. */
        double energyCost;

        /**
         * Default constructor. Initialize all counters and flags.
         */
        EnergyAwareReplData(int freq_bits, int write_bits) 
            : lastTouchTick(0), accessFreq(freq_bits), writeCount(write_bits),
              bytesUsed(0), isDirty(false), predictedReuse(0), energyCost(0.0) {}
    };

  private:
    /** Number of bits for the access frequency counter. */
    const int frequencyBits;
    
    /** Number of bits for the write counter. */
    const int writeBits;
    
    /** Weight for recency factor in cost function. */
    const double recencyWeight;
    
    /** Weight for frequency factor in cost function. */
    const double frequencyWeight;
    
    /** Weight for write intensity factor in cost function. */
    const double writeWeight;
    
    /** Weight for dirty bit factor in cost function. */
    const double dirtyWeight;
    
    /** Weight for utilization factor in cost function. */
    const double utilizationWeight;
    
    /** PCM write energy cost multiplier. */
    const double pcmWriteCost;
    
    /** PCM read energy cost multiplier. */
    const double pcmReadCost;
    
    /** Cache block size for utilization calculation. */
    const uint32_t blockSize;

    /**
     * Calculate energy-aware cost function for a cache block.
     * 
     * @param repl_data Replacement data for the block
     * @return Energy-aware cost value (lower is better for retention)
     */
    double calculateEnergyCost(const std::shared_ptr<EnergyAwareReplData>& repl_data) const;

  public:
    /** Convenience typedef. */
    typedef EnergyAwareRPParams Params;

    /**
     * Construct and initialize this replacement policy.
     */
    EnergyAwareRP(const Params *p);

    /**
     * Destructor.
     */
    ~EnergyAwareRP() {}

    /**
     * Invalidate replacement data to set it as the next probable victim.
     * Resets all counters and energy cost.
     *
     * @param replacement_data Replacement data to be invalidated.
     */
    void invalidate(const std::shared_ptr<ReplacementData>& replacement_data)
                                                              const override;

    /**
     * Touch an entry to update its replacement data.
     * Updates access frequency, recency, and recalculates energy cost.
     *
     * @param replacement_data Replacement data to be touched.
     */
    void touch(const std::shared_ptr<ReplacementData>& replacement_data) const
                                                                     override;

    /**
     * Reset replacement data. Used when an entry is inserted.
     * Initializes all counters and energy cost.
     *
     * @param replacement_data Replacement data to be reset.
     */
    void reset(const std::shared_ptr<ReplacementData>& replacement_data) const
                                                                     override;

    /**
     * Find replacement victim using energy-aware cost function.
     * Selects the block with the highest energy cost (most beneficial to evict).
     *
     * @param candidates Replacement candidates, selected by indexing policy.
     * @return Replacement entry to be replaced.
     */
    ReplaceableEntry* getVictim(const ReplacementCandidates& candidates) const
                                                                     override;

    /**
     * Instantiate a replacement data entry.
     *
     * @return A shared pointer to the new replacement data.
     */
    std::shared_ptr<ReplacementData> instantiateEntry() override;

    /**
     * Update write statistics for energy calculation.
     * 
     * @param replacement_data Replacement data to update
     */
    void updateWriteStats(const std::shared_ptr<ReplacementData>& replacement_data) const;

    /**
     * Update block utilization information.
     * 
     * @param replacement_data Replacement data to update
     * @param bytes_accessed Number of bytes accessed in this block
     */
    void updateUtilization(const std::shared_ptr<ReplacementData>& replacement_data, 
                          uint32_t bytes_accessed) const;

    /**
     * Set dirty bit status for write-back cost calculation.
     * 
     * @param replacement_data Replacement data to update
     * @param is_dirty Whether the block is dirty
     */
    void setDirtyStatus(const std::shared_ptr<ReplacementData>& replacement_data, 
                       bool is_dirty) const;
};

#endif // __MEM_CACHE_REPLACEMENT_POLICIES_ENERGY_AWARE_RP_HH__
