# IoTBench

## Download

* git clone https://github.com/BenchCouncil/IoTBench/

## Compile

* `make $isa SID=$SID`

  > * `$isa` can be riscv or arm;
  > * `$SID` is the name of the subspace;
  > * The name of the obtained file is `iotbench-$ISA-$SID`
  > * Example: run `make riscv SID=A `, get `iotbench-RISCV-A`

## Parameters

The parameters below can be changed according to the scenario.

**`benchmark.h`**

* `TOTAL_DATA_SIZE` is the data size;
* `TEST_TYPE` is the data type;
* `DATA_DIM` is the data dimension;
* `CALCULATE_ACCURACY` is the accuracy of the calculation;
* `NUM_ITERATE` represents the number of iterations.

