# load the function
source("analysis_code.R")

# run the function
df <- distanceSensor(file = "10181048.CSV", preTol = 10, postTol = 5)

# now report the data
print(paste0("For the period between ", 
             substring(df$data$date_time[1], nchar(as.character(df$data$date_time[1]))-8+1),
             " and ",
             substring(df$data$date_time[dim(df$data)[1]], nchar(as.character(df$data$date_time[dim(df$data)[1]]))-8+1),
             " the number of recorded passes was ",
             dim(df$periods)[1],
             ", for which the passing distance was ",
             round(mean(df$periods$dist)),
             " ± ",
             round(sd(df$periods$dist)),
             " cm."
)
)

# and print a histogram
hist(df$periods$dist, 
     xlab = "Distance [cm]", 
     ylab = "Frequency [n]", 
     main = paste0(substring(df$data$date_time[1], nchar(as.character(df$data$date_time[1]))-8+1),
                   " to ",
                   substring(df$data$date_time[dim(df$data)[1]], nchar(as.character(df$data$date_time[dim(df$data)[1]]))-8+1)
     )
)
