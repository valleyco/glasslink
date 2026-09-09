idf_component_register(
    SRCS "src/contract_parse.c" "src/contract_apply.c"
    INCLUDE_DIRS "include"
    REQUIRES codec render
)
