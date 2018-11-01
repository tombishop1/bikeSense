# This is an R function for analysing the data from the
# Bicycle Distance Sensor which is described at
# https://thomasbishop.uk/bicycle-distance-sensor
# it searches for button presses, and calculates the
# nearest distance sensed near the time of that press

# the preTol parameter is the time you'd like to open the search window BEFORE the button is pressed
# the postTol parameter is the time you'd like to close the search window AFTER the button is released
# the units for those parameters are in tenths of a second

distanceSensor <- function(file = "10181048.CSV", preTol = 10, postTol = 5){

# import the data
data <- read.csv( file, header = TRUE )
data$seconds <- round(data$milliseconds/1000, 1)
data$button <- as.logical(data$button)

# make a list of when the button changes
changes <- c(1,1+which(diff(data$button)!=0))

# now put them into pairs, with a list of starts and ends
periods <- data.frame(
  starts = changes[c(TRUE, FALSE)],
  ends = c(changes[c(FALSE, TRUE)], dim(data)[1])
)
rm(changes)

# now extend the presses by the tolerances defined below
# This will extend the begining of the mark and end of the mark
# We probably want the beginning of the mark extension to be longer,
# as we'll usually press the button after a car has passed!

# you'll need to modify these to suit how you use the marker
# preTol & postTol the input here is in tenths of a second, so 10 = 1 second
periods$starts_ext <- periods$starts - preTol
periods$ends_ext   <- periods$ends + postTol
periods$starts_ext[periods$starts_ext < 0] <- periods$starts[periods$starts_ext < 0]
periods$ends_ext[periods$ends_ext > dim(data)[1]] <- dim(data)[1]
rm(preTol, postTol)

# now calculate the minimum distance in each of the periods
for (x in seq(1:dim(periods)[1])){
periods$dist[x] <- min(data$distance[periods$starts_ext[x] : periods$ends_ext[x]])
}
rm(x)

return(list(periods = periods,
            data = data))
}


