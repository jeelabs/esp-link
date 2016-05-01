#include "web-server.h"

#include "espfs.h"
#include "config.h"

char * webServerPages = NULL;

char * ICACHE_FLASH_ATTR webServerUserPages()
{
	return webServerPages;
}

void ICACHE_FLASH_ATTR webServerBrowseFiles()
{
	char buffer[1024];
	buffer[0] = 0;
	
	EspFsIterator it;
	espFsIteratorInit(userPageCtx, &it);
	{
		while( espFsIteratorNext(&it) )
		{
			int nlen = strlen(it.name);
			if( nlen >= 6 )
			{
				if( os_strcmp( it.name + nlen-5, ".html" ) == 0 )
				{
					char sh_name[17];
					
					int spos = nlen-5;
					
					while( spos > 0 )
					{
						if( it.name[spos+1] == '/' )
							break;
						spos--;
					}
					
					int ps = nlen-5-spos;
					if( ps > 16 )
						ps = 16;
					os_memcpy(sh_name, it.name + spos, ps);
					sh_name[ps] = 0;
					
					os_strcat(buffer, ", \"");
					os_strcat(buffer, sh_name);
					os_strcat(buffer, "\", \"/");
					os_strcat(buffer, it.name);
					os_strcat(buffer, "\"");
				}
			}
			if( strlen(buffer) > 600 )
				break;
		}
	}
	
	if( webServerPages != NULL )
		os_free( webServerPages );
	
	int len = strlen(buffer) + 1;
	webServerPages = (char *)os_malloc( len );
	os_memcpy( webServerPages, buffer, len );
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

