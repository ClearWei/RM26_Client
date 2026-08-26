#include "robomaster.pb.h"
#include <iostream>

int main() {
  robomaster::VideoControl vc;
  vc.set_video_url("test");
  std::cout << vc.video_url() << std::endl;
  return 0;
}
