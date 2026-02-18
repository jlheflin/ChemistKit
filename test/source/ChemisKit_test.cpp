#include "lib.hpp"

auto main() -> int
{
  auto const lib = library {};

  return lib.name == "ChemisKit" ? 0 : 1;
}
