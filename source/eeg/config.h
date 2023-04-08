/**
 * 
 * \file config.h
 * \brief The header file for module handling the configuration.
 *
 * 
 * $Id: config.h 74 2013-01-22 16:29:42Z jakab $
 */

# ifndef __CONFIG_H__
# define __CONFIG_H__

//---------------------------------------------------------------------------
//   								Prototypes
//---------------------------------------------------------------------------
static BOOL		config_CreateAppdataDirStructure(void);
void			config_init(int intScreenXResolution, int intScreenYResolution, int intScreenWidth, int intScreenHeight);
void			config_load(CONFIGURATION * pcfgConfiguration);
void			config_store (CONFIGURATION cfgConfiguration);

#endif