/*
  This is special.h

  Copyright (C) 2004,2005 Fokko du Cloux
  part of the Atlas of Reductive Lie Groups

  For copyright and license information see the LICENSE file
*/

#ifndef SPECIAL_H  /* guard against multiple inclusions */
#define SPECIAL_H

#include "commands.h"
#include "emptymode.h"
#include "mainmode.h"
#include "realmode.h"
#include "blockmode.h"

/******** function declarations ********************************************/

namespace atlas {

namespace special {

  void addSpecialCommands(commands::CommandMode&, emptymode::EmptymodeTag);
  void addSpecialCommands(commands::CommandMode&, mainmode::MainmodeTag);
  void addSpecialCommands(commands::CommandMode&, realmode::RealmodeTag);
  void addSpecialCommands(commands::CommandMode&, blockmode::BlockmodeTag);

  void addSpecialHelp(commands::CommandMode&, commands::TagDict&,
		      emptymode::EmptymodeTag);
  void addSpecialHelp(commands::CommandMode&, commands::TagDict&,
		      mainmode::MainmodeTag);
  void addSpecialHelp(commands::CommandMode&, commands::TagDict&,
		      realmode::RealmodeTag);
  void addSpecialHelp(commands::CommandMode&, commands::TagDict&,
		      blockmode::BlockmodeTag);
}

}

#endif
