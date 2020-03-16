# Linux
`./build.sh` will hopefully build both `hatlag` and `hatser`.

## Running
A kernel with `process_vm_readv` support is needed (added in 3.2). Additionally, depending on permission settings for your setup, you may need to run the programs as root to allow for reading the game's memory and/or `/dev/input/`.