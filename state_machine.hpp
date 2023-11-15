#pragma once

#include <cassert>

#define SM_ENTER_STATE(state_machine_name, state_name) goto state_machine_label_##state_name##_for_##state_machine_name

#define SM_DEFINE_STATE_BEGIN(state_machine_name, state_name) \
  do {                                                        \
    state_machine_label_##state_name##_for_##state_machine_name : {


      
#define SM_END_STATE()                                       \
  }                                                          \
  assert(false && "state machine state can't fall through"); \
  }                                                          \
  while (0)
