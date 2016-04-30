#include "web-server.h"

#include "espfs.h"
#include "config.h"

void ICACHE_FLASH_ATTR webServerBrowseFiles()
{
	EspFsIterator it;
	espFsIteratorInit(userPageCtx, &it);
	{
		while( espFsIteratorNext(&it) )
		{
			if( strlen(it.name) >= 6 )
			{
				if( os_strcmp( it.name + strlen(it.name)-5, ".html" ) == 0 )
				{
					os_printf("%s\n", it.name); // TODO
				}
			}
		}
	}
}

void ICACHE_FLASH_ATTR webServerInit()
{
	espFsInit(userPageCtx, (void *)getUserPageSectionStart(), ESPFS_FLASH);
	if( espFsIsValid( userPageCtx ) ) {
		os_printf("Valid user file system found!\n");
		webServerBrowseFiles();
	}
	else
		os_printf("No user file system found!\n");
}

