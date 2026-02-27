import matplotlib.pyplot as plt
import csv

Names = []
Values = []
rowNum = 0;
TAKE_EVERY=15
UP_TO=500000

# column indices
UCT_TIMESTAMP=0 # string
ELECTRICITY_LOAD=1 # float
RESIDENTIAL_ELECTRICITY_PRICE=2 # float
RESIDENTIAL_SOLAR_GENERATION=3
RESIDENTIAL_WIND_GENERATION=4
TEMPERATURE=5
RELATIVE_HUMIDITY=6


with open('../dataset/Dataset.csv','r') as csvfile:
    lines = csv.reader(csvfile, delimiter=',')

    # skip the first row (titls)
    next(lines)

    # 201 600
    for row in lines:

        # take every 10th row
        if rowNum % TAKE_EVERY == 0:
            # HUMIDITY -> WIND AND SOLAR = weak
            # Names.append(float(row[RELATIVE_HUMIDITY]))
            # Values.append(float(row[RESIDENTIAL_WIND_GENERATION]) + float(row[RESIDENTIAL_SOLAR_GENERATION]))

            # HUMIDITY -> WIND = weak
            # Names.append(float(row[RELATIVE_HUMIDITY]))
            # Values.append(float(row[RESIDENTIAL_WIND_GENERATION]))


            # TEMPERATURE -> HUMIDITY = impossible
            # Names.append(float(row[TEMPERATURE]))
            # Values.append(float(row[RELATIVE_HUMIDITY]))


            # TEMPERATURE -> SOLAR = impossible
            # Names.append(float(row[TEMPERATURE]))
            # Values.append(float(row[RESIDENTIAL_SOLAR_GENERATION]))

            # PRICE -> LOAD = strong 
            # Names.append(float(row[ELECTRICITY_LOAD]))
            # Values.append(float(row[RESIDENTIAL_ELECTRICITY_PRICE]))


            # PRICE -> Renewable generation = mountain, check this!!!!
            # Names.append(float(row[RESIDENTIAL_ELECTRICITY_PRICE]))
            # Values.append(float(row[RESIDENTIAL_SOLAR_GENERATION]) + float(row[RESIDENTIAL_WIND_GENERATION]))


            # PRICE -> Solar generation 
            # Names.append(float(row[RESIDENTIAL_ELECTRICITY_PRICE]))
            # Values.append(float(row[RESIDENTIAL_SOLAR_GENERATION]))


            # PRICE -> Wind generation 
            Names.append(float(row[RESIDENTIAL_ELECTRICITY_PRICE]))
            Values.append(float(row[RESIDENTIAL_WIND_GENERATION]))


        rowNum = rowNum + 1
        if (rowNum >= UP_TO):
            break
        

plt.scatter(Names, Values, color = 'g',s = 3)
plt.xticks(rotation = 25)

plt.ylabel('') # float
plt.xlabel('HUM') # string

plt.show()
