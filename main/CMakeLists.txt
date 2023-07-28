idf_component_register(SRCS  "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES    modbus_master_provide
                                mqtt_provide
                    )
