target_sources(app PRIVATE ble.c debug.c hog2.c main.c sensor.c settings.c)
target_include_directories(app PRIVATE .)
# hog.c revsvc.c settings.c power.c sensor.c main.c button.c 