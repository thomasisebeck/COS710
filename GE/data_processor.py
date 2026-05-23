import math
# import matplotlib.pyplot as plt
import csv
from collections import deque
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

MONTHS_IN_YEAR = 12
DAYS_IN_YEAR = 365
PREV_DAY_LAG = 96
MINUTES_IN_HOUR = 60
MINUTES_IN_DAY = 24 * MINUTES_IN_HOUR
load_n1 = 0
load_n2 = 0
load_n3 = 0
load_n4 = 0
load_n5 = 0
load_n6 = 0
prev_day_load = 0

max_load = 0
min_load = 10000

lag_lines = 6

curr_day = 0

prev_day_buffer = deque(maxlen=PREV_DAY_LAG)

IN_PATH = "./dataset/Dataset.csv"
OUT_PATH = './dataset/processed.csv'

with open(IN_PATH,'r') as csvfile:
    lines = csv.reader(csvfile, delimiter=',')

    # skip the first row (titles)
    next(lines)

    # 201 600
    for row in lines:

            load = float(row[ELECTRICITY_LOAD])

            if (load > max_load):
                max_load = load

            if (load < min_load): 
                min_load = load

with open(IN_PATH,'r') as csvfile:
    lines = csv.reader(csvfile, delimiter=',')

    # skip the first row (titles)
    next(lines)

    # 201 600
    for row in lines:

            # Parse timestamp
            dt = datetime.strptime(row[UCT_TIMESTAMP], "%d/%m/%Y %H:%M")

            day_of_year = dt.timetuple().tm_yday;

            day_pi_scaled =  2 * math.pi * (day_of_year / DAYS_IN_YEAR ); 

            # maps each point on the unit circle, so that 
            # sin(pi) and sin(2pi) is not the same
            # because cos differs by 90deg
            normalised_day_of_year_cos = math.cos( day_pi_scaled );
            normalised_day_of_year_sin = math.sin( day_pi_scaled );

            minutes = dt.hour * MINUTES_IN_HOUR + dt.minute;

            minutes_pi_scaled = 2 * math.pi * ( minutes / MINUTES_IN_DAY )

            normalised_minute_cos = math.cos( minutes_pi_scaled )
            normalised_minute_sin = math.sin( minutes_pi_scaled )

            load = float(row[ELECTRICITY_LOAD])
            load_min_max_scaled = ( load - min_load) / ( max_load - min_load )

            if len(prev_day_buffer) == PREV_DAY_LAG:
                prev_day_load = prev_day_buffer[0]
            else:
                prev_day_load = None

            if prev_day_load is not None:

                ToWrite.append({
                    'load': load_min_max_scaled,
                    'load_n1': load_n1,
                    'load_n2': load_n2,
                    'load_n3': load_n3,
                    'load_n4': load_n4,
                    'load_n5': load_n5,
                    'load_n6': load_n6,
                    'load_prev_day': prev_day_load,
                    'normalised_day_of_year_cos': normalised_day_of_year_cos,
                    'normalised_day_of_year_sin': normalised_day_of_year_sin,
                    'normalised_minute_cos': normalised_minute_cos,
                    'normalised_minute_sin': normalised_minute_sin
                })

            load_n6 = load_n5
            load_n5 = load_n4
            load_n4 = load_n3
            load_n3 = load_n2
            load_n2 = load_n1
            load_n1 = load_min_max_scaled

            prev_day_buffer.append(load_min_max_scaled)
  
# write to a csv file
with open(OUT_PATH, 'w', newline='') as csvfile:
    fieldnames=['load','load_n1',
                'load_n2',
                'load_n3',
                'load_n4',
                'load_n5',
                'load_n6',
                'load_prev_day',
                'normalised_day_of_year_cos',
                'normalised_day_of_year_sin',
                'normalised_minute_cos',
                'normalised_minute_sin',
                ]
    writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
    writer.writeheader()
    writer.writerows(ToWrite)


# plt.scatter(Names, Values, color = 'g',s = 3)
# plt.xticks(rotation = 25)

# plt.ylabel('load') # float
# plt.xlabel('time_index') # string

# plt.show()
