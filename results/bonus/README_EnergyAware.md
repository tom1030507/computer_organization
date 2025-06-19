# Energy-Aware Last Level Cache (LLC) Replacement Policy

## 概述

本專案實現了一個專門針對PCM（Phase Change Memory）基礎主記憶體系統設計的能耗感知最後一級快取替換策略。該策略使用多因子成本函數來優化快取決策，以減少整體系統能耗。

## 設計理念

### 為什麼需要能耗感知的替換策略？

1. **PCM特性**: PCM相比DRAM具有更高的寫入能耗（約10-15倍）但具有非揮發性
2. **寫入不對稱**: PCM的讀取和寫入能耗差異巨大，需要特別考慮寫入操作
3. **持久性考慮**: 快取驅逐決策會直接影響到主記憶體的寫入頻率
4. **性能與能耗平衡**: 需要在快取命中率和能耗消耗之間找到最佳平衡點

### 能耗感知成本函數

我們的替換策略使用以下多維度成本函數：

```
Energy_Cost = α₁×Recency_Factor + α₂×Frequency_Factor + α₃×Write_Intensity + 
              α₄×Dirty_Factor + α₅×Utilization_Factor + 
              β₁×Future_Access_Cost - β₂×WriteBack_Cost
```

#### 成本因子說明：

1. **Recency Factor (時間局部性)**
   - 基於LRU時間戳
   - 越久未使用的塊驅逐成本越低
   - 權重: α₁ = 0.25-0.3

2. **Frequency Factor (訪問頻率)**
   - 使用飽和計數器追蹤訪問頻率
   - 低頻率訪問的塊優先驅逐
   - 權重: α₂ = 0.25-0.35

3. **Write Intensity (寫入強度)**
   - 追蹤寫入操作頻率
   - 高寫入強度塊考慮PCM寫入能耗
   - 權重: α₃ = 0.2-0.25

4. **Dirty Factor (髒位狀態)**
   - 考慮寫回操作的PCM寫入成本
   - 髒塊驅逐需要額外的寫入能耗
   - 權重: α₄ = 0.1-0.15

5. **Utilization Factor (空間局部性)**
   - 追蹤快取行內實際使用的位元組數
   - 低利用率塊優先驅逐
   - 權重: α₅ = 0.05-0.1

6. **Future Access Cost (未來訪問成本)**
   - 基於訪問模式預測未來PCM訪問成本
   - 係數: β₁ = 0.1

7. **WriteBack Cost (寫回成本)**
   - 驅逐髒塊的PCM寫入成本
   - 係數: β₂ = 0.2

## 檔案結構

```
gem5/src/mem/cache/replacement_policies/
├── energy_aware_rp.hh          # 標頭檔案
├── energy_aware_rp.cc          # 實現檔案
├── ReplacementPolicies.py      # Python配置類別（已更新）
└── SConscript                  # 構建配置（已更新）
```

## 主要特性

### 1. 動態能耗估算
- 即時計算每個快取塊的能耗成本
- 考慮PCM讀寫能耗不對稱性
- 預測未來訪問模式

### 2. 多維度決策
- 結合時間和空間局部性
- 考慮寫入操作特殊性
- 平衡性能和能耗

### 3. 可配置參數
- 權重因子可調整
- PCM能耗參數可設定
- 計數器位數可配置

### 4. 硬體友好設計
- 使用飽和計數器（硬體易實現）
- 增量更新成本函數
- 低計算複雜度

## 使用方法

### 1. 編譯gem5
```bash
cd gem5
scons build/X86/gem5.opt -j8
```

### 2. 配置替換策略
```python
# 在Python配置檔案中
l3_cache.replacement_policy = EnergyAwareRP()

# 調整參數
l3_cache.replacement_policy.recency_weight = 0.25
l3_cache.replacement_policy.frequency_weight = 0.35
l3_cache.replacement_policy.write_weight = 0.25
l3_cache.replacement_policy.pcm_write_cost = 15.0
l3_cache.replacement_policy.pcm_read_cost = 1.0
```

## 參數調整指南

### 針對不同工作負載的參數建議：

#### 寫入密集型工作負載
```python
recency_weight = 0.2
frequency_weight = 0.3
write_weight = 0.35        # 提高寫入權重
dirty_weight = 0.1
utilization_weight = 0.05
```

#### 讀取密集型工作負載
```python
recency_weight = 0.3
frequency_weight = 0.4     # 提高頻率權重
write_weight = 0.15
dirty_weight = 0.1
utilization_weight = 0.05
```

#### 混合工作負載
```python
recency_weight = 0.25
frequency_weight = 0.35
write_weight = 0.25
dirty_weight = 0.1
utilization_weight = 0.05
```

## 與其他策略的比較

| 策略 | 考慮因素 | PCM優化 | 複雜度 | 適用場景 |
|------|----------|---------|--------|----------|
| LRU | 時間局部性 | 無 | 低 | 通用 |
| LFU | 訪問頻率 | 無 | 中 | 高重用 |
| RRIP | 重用預測 | 無 | 中 | 掃描友好 |
| **EnergyAware** | **多維度** | **是** | **中高** | **PCM系統** |

## 預期效果

1. **能耗降低**: 相比傳統LRU，預期PCM寫入能耗降低15-25%
2. **性能保持**: 在大多數工作負載下保持相近的命中率
3. **寫入優化**: 減少不必要的PCM寫入操作
4. **適應性**: 能夠適應不同的訪問模式

## 未來改進方向

1. **機器學習增強**: 使用ML預測訪問模式
2. **動態權重調整**: 根據工作負載特性動態調整權重
3. **多級優化**: 考慮L1/L2快取的協同優化
4. **溫度感知**: 考慮PCM的溫度特性
5. **磨損均衡**: 集成PCM磨損均衡機制

## 技術實現細節

### 核心類別結構
- `EnergyAwareRP`: 主要替換策略類別
- `EnergyAwareReplData`: 替換數據結構，包含所有追蹤信息
- 飽和計數器用於硬體友好的頻率和寫入計數

### 成本函數計算
- 即時計算每個快取塊的能耗成本
- 動態更新權重和參數
- 硬體實現友好的設計

---

**注意**: 本專案為學術研究用途，針對PCM基礎記憶體系統的能耗優化設計。
