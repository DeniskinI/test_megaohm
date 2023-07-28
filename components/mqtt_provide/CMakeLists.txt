idf_component_register(
    SRCS        mqtt_provide

    INCLUDE_DIRS . ..
    REQUIRES    nvs_flash
                mqtt
                protocol_examples_common
                modbus_master_provide


    EMBED_TXTFILES      aws.crt
                        client.crt
                        client.key

)

target_compile_options(${COMPONENT_LIB} PRIVATE -DESP_IDF)
                       
