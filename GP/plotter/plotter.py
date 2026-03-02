import matplotlib.pyplot as plt
import csv
from datetime import datetime

Names = []
Values = []
ToWrite = [];
rowNum = 0;
TAKE_EVERY=1
UP_TO=500000

# column indices
UCT_TIMESTAMP=0 # string
ELECTRICITY_LOAD=1 # float
RESIDENTIAL_ELECTRICITY_PRICE=2 # float
RESIDENTIAL_SOLAR_GENERATION=3
RESIDENTIAL_WIND_GENERATION=4
TEMPERATURE=5
RELATIVE_HUMIDITY=6

# min: cell E2
# =MIN(A2:A201605)
# max: cell E3
# =MAX(A2:A201605)

# min_max_scaled_load
# =(A2-$E$2)/($E$3-$E$2)

with open('../dataset/Dataset.csv','r') as csvfile:
    lines = csv.reader(csvfile, delimiter=',')

    # skip the first row (titles)
    next(lines)

    # 201 600
    for row in lines:

        # take every 10th row
        if rowNum % TAKE_EVERY == 0:
            # HUMIDITY -> WIND AND SOLAR = weak
            # Names.append(float(row[RELATIVE_HUMIDITY]))
            # Values.append(float(row[RESIDENTIAL_WIND_GENERATION]) + float(row[RESIDENTIAL_SOLAR_GENERATION]))

              # Parse timestamp
            dt = datetime.strptime(row[UCT_TIMESTAMP], "%d/%m/%Y %H:%M")

            # Largest timestamp: 23:45
            # Smallest timestamp: 00:00

            # lagest minute = 23 * 60 + 45 = 1425
            # largest index = 1425 / 15 = 95

            MINUTES_IN_HOUR = 60
            TIME_INTERVAL_MINUTE = 15
            LARGEST_INDEX = 95
            
            time_in_minutes = (dt.hour * MINUTES_IN_HOUR) + dt.minute

            # convert to minutes * 15 (range: 0 to 95)
            time_index = time_in_minutes / TIME_INTERVAL_MINUTE

            normalised_time = time_index / LARGEST_INDEX
    
            Names.append(time_index)
            Values.append(float(row[ELECTRICITY_LOAD]))
            ToWrite.append({'load': float(row[ELECTRICITY_LOAD]), 'time_index_normalised': normalised_time })

           # Names.append(row[UCT_TIMESTAMP])
           # Values.append(float(row[ELECTRICITY_LOAD]))

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
            # Names.append(float(row[RESIDENTIAL_ELECTRICITY_PRICE]))
            # Values.append(float(row[RESIDENTIAL_WIND_GENERATION]))


        rowNum = rowNum + 1
        if (rowNum >= UP_TO):
            break
        
# write to a csv file

with open('../dataset/processed.csv', 'w', newline='') as csvfile:
    fieldnames=['load','time_index']
    writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
    writer.writeheader()
    writer.writerows(ToWrite)


# plt.scatter(Names, Values, color = 'g',s = 3)
# plt.xticks(rotation = 25)

# plt.ylabel('load') # float
# plt.xlabel('time_index') # string

# plt.show()
