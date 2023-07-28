idf_component_register(
    SRCS mb_m_provide.c
    INCLUDE_DIRS .

    REQUIRES    freemodbus
)

target_compile_options(${COMPONENT_LIB} PRIVATE -DESP_IDF)
                       
