# cs4760_Assignment5

    Currently I am only seeing the program see pages as already in a frame. Initially I assumed this was because of a difficulty getting the results I wanted when relying on random chance. However, I have reduced the number of frames and increased the amount of time individual processes are in memory to increase the chance of a page fault. Even with these changes I have seen no change. This indicates to me that I have an error somewhere. I am not sure exactly what it is.

    Second chance FIFO queue is implemented here, however dirty bit optimisation is not. 

    There was a good while where the message queue was not being closed properly when the program exited. However, I believe this is now fixed.