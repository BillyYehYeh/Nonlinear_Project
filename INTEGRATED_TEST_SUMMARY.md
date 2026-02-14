# Divider_PreCompute + Divider Integrated Test Summary

## Changes Made

### 1. **Modified Divider_test.cpp**
   - Integrated both `Divider_PreCompute.cpp` and `Divider.cpp` modules into a single testbench
   - Created interconnections between the two modules:
     - `Divider_PreCompute_Module::Leading_One_Pos` → `Divider_Module::ks` (4-bit)
     - `Divider_PreCompute_Module::Mux_Result` → `Divider_Module::Mux_Result` (16-bit)
   - Changed test input from direct `ky` and `ks` parameters to:
     - **32-bit `input`** → Divider_PreCompute (computes Leading_One_Pos and Mux_Result internally)
     - **4-bit `ky`** → Divider (user provided)
   - Updated all 28 test cases with specific 32-bit input values
   - Enhanced output formatting to show computed `ks` values from PreCompute

### 2. **Fixed Bug in Divider_PreCompute.cpp**
   - Changed: `Mux_Result_Result.write(threshold);` 
   - To: `Mux_Result.write(threshold);`
   - This was preventing proper port connection to the Divider module

### 3. **Fixed Bug in Divider_PreCompute_test.cpp**
   - Changed: `dut->Threshold_Result(threshold_result_sig);`
   - To: `dut->Mux_Result(threshold_result_sig);`
   - Corrected port name to match actual module interface

## Test Configuration

### Module Chain
```
Testbench Input (32-bit)
    ↓
Divider_PreCompute_Module
    ├→ Leading_One_Pos (4-bit) ─→ Divider_Module.ks
    └→ Mux_Result (16-bit) ─────→ Divider_Module.Mux_Result
    
Testbench Input (4-bit ky)
    ↓
Divider_Module
    ↓
Divider_Output (16-bit)
```

### Test Coverage (28 total tests)
- **Edge Cases**: Leading_One_Pos = 0-1 (4 tests)
- **Small Values**: Leading_One_Pos = 2-4 (6 tests)
- **Medium Values**: Leading_One_Pos = 5-8 (4 tests)
- **Large Values**: Leading_One_Pos = 9-15 (5 tests)
- **Mixed Combinations**: Various ky/ks ratios (5 tests)
- **Boundary Cases**: s = 0 and s = 1 (4 tests)

## Test Results

### Key Statistics
- **Total Tests Executed**: 28
- **Average Error Percentage**: 24364.30% (includes underflow cases)
- **Average Error (Excluding Saturation)**: 18.28%
- **Best Case Error**: 0.51%
- **Worst Case Error**: 667100.00% (extreme underflow)
- **Tests without Saturation**: 20
- **Tests with Underflow**: 8

### Error Breakdown

**Low Error (<5%)**:
- Test 4: 3.46%
- Test 8: 0.62%
- Test 12: 2.22%
- Test 21: 0.51%

**Acceptable Error (5%-20%)**:
- Tests 1, 2, 3, 6, 7, 10, 13, 20, 22, 26, 28: Range 6.32%-19.99%

**Moderate Error (20%-50%)**:
- Tests 5, 9, 11, 15, 25, 27: Range 29.02%-43.21%

**Underflow Cases (>50%)**:
- Tests 14, 16, 17, 18, 19, 23, 24: Show underflow due to excessive right-shift

## Output Details

Each test displays:
1. **Input Signals**: ky, ks_computed, s, Mux_Result (hex, binary, components)
2. **Golden Data Calculation**: Mathematical expected values
3. **Internal Signals**: Computation steps (right_shift, exponent calculations)
4. **Output Signals**: Divider_Output with binary representation and float conversion
5. **Error Percentage**: Calculated from golden data vs actual output

## Build & Run Instructions

```bash
# Clean and build
cd /mnt/e/BillyFolder/Code/CNN
rm -rf build && mkdir -p build && cd build
cmake ..
make

# Run integrated test
./Divider_test

# Output stored in
./divider_test_output.txt
```

## Key Features of the Modified Test

✅ **Full integration** of both modules with automatic connections
✅ **Realistic testing** with 32-bit inputs that generate meaningful ks values
✅ **Comprehensive coverage** of edge cases, boundaries, and stress conditions
✅ **Detailed output formatting** showing all computation stages
✅ **Error analysis** with golden data comparison
✅ **Saturation detection** identifying overflow/underflow conditions
✅ **Summary statistics** for overall performance analysis

