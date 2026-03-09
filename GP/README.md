# prerequisites

sudo pacman -S glibc
sudo pacman -S debuginfod

# libraries

CSV parser (for parsing csv file)

`git clone https://github.com/vincentlaucsb/csv-parser`

# NOTES

Given the dataset we will be looking at this as a time series problem rather than predicting a dependent variable given a set of independent variables. You only need to use the “Electricty Load” column. Use genetic programming to predict the electricity load for a particular time on a particular day given n previous values. Alternatively, the m values at the same time on different days can be used.
