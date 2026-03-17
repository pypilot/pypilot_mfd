idf_component_register(SRCS "main.c" "../../common/lis2dw.c" "../../common/web.c"  "../../common/wireless.c" "../../common/settings.c" "../../common/dhcp.c"
	INCLUDE_DIRS "."
	EMBED_TXTFILES index.html
)
