# Run using docker 

rm -rf build
docker build -t gp-engine .
docker run --rm gp-engine

# Run without docker

## prerequisite packages 

sudo pacman -S glibc
sudo pacman -S debuginfod

## libraries

CSV parser (for parsing csv file)

Clone this if it does not exist:
`git clone https://github.com/vincentlaucsb/csv-parser`

## how do run

Required compiler: GNU 15.2.1

```
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make
./out
```
