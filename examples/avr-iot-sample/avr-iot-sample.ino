#include <string.h>
#include <stdlib.h>
#include "log.h"
extern void demo_setup(void);
extern void demo_loop(void);
void setup() {
#if 0
  Log.begin(115200);
  Log.info("started");
  delay(200);
  { // scope block
  int r = rand();
  size_t s = r == 1 ? 499 : 500;
  char dummy_stack_buffer[500];
  memset(dummy_stack_buffer, 'A', sizeof(dummy_stack_buffer));
  Log.infof("Dummy stack check %c\r\n", dummy_stack_buffer[1]);
  Log.infof("Dummy stack check %c\r\n", dummy_stack_buffer[399]);
  }
#endif
  demo_setup();
}
void loop() {
  demo_loop();
}
