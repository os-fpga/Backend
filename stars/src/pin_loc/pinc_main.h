#pragma once
#ifndef __rs_PIN_LOC__PINC_MAIN_H_
#define __rs_PIN_LOC__PINC_MAIN_H_

namespace pinc {

class cmd_line;

// entry point for libpinconst.a
inline int pinc_main(const cmd_line& cmd) { return 0; }

}

#endif

