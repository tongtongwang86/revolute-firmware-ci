target_sources(app PRIVATE ble.c main.c sensor.c settings.c statemanager.c pwmled.c power.c button.c revsvc.c hog.c)
target_include_directories(app PRIVATE .)
# hog.c revsvc.c settings.c power.c sensor.c main.c button.c 