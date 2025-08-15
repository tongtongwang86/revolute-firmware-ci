target_sources(app PRIVATE ble.c main.c sensor.c settings.c statemanager.c pwmled.c power.c button.c revsvc.c hog.c haptic.c )
target_include_directories(app PRIVATE .)
# charger.c podprotocol.c
# hog.c revsvc.c settings.c power.c sensor.c main.c button.c 