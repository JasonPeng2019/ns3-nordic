# WARNING

This module will no longer be maintained. The original creation was part of a master-thesis study, performed in 2017-2018.

The module is provided as-is without any guarantees.

# BUILD INSTRUCTIONS
Verified with [ns-3-dev](https://gitlab.com/nsnam/ns-3-dev) on 2021-06-14 with commit [25e0d01d](https://gitlab.com/nsnam/ns-3-dev/-/commit/25e0d01d2883ae6da3c18a4d926f6b9aa2b99128) 

## Setup
Clone this repository

```
git clone https://gitlab.com/Stijng/ns3-ble-module.git
```

Clone ns-3-dev and checkout the desired version

```
git clone https://gitlab.com/nsnam/ns-3-dev.git
git checkout 25e0d01d2883ae6da3c18a4d926f6b9aa2b99128
```

Place the `ble` folder of the ns3-ble-module inside the `src` folder of the ns-3-dev repo. This can be done with a copy or a symlink:

```
cd ns-3-dev/src
ln -s -v ../../ns3-ble-module/ble/ ble
```

## Configure and build
Configure ns3 with the desired options using the `waf` build system from the ns-3-dev root:

```
cd ns-3-dev
./waf configure --enable-tests --enable-examples
```

followed by a build command
```
./waf build
```

## Run
You can run examples with `--run`

```
./waf --run ble-routing-dsdv
```

