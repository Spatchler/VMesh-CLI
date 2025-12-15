#include "progressBar.hpp"

void printProgressBar(float pProgress, const std::string& pTitle, uint8_t pPrimaryEscapeColour, uint8_t pSecondaryEscapeColour, uint pWidth) {
  std::print("\x1b[1F\x1b[2K{} ", pTitle);
  float threshold = std::ceil(pProgress * pWidth);
  std::print("\x1b[{}m\x1b[{}m", pPrimaryEscapeColour, pPrimaryEscapeColour + 10);
  for (uint i = 0; i < threshold; ++i) {
    std::print(" ");
  }
  if (threshold <= pWidth - 1) {
    std::print("\x1b[{}m\x1b[{}m", pSecondaryEscapeColour + 10, pSecondaryEscapeColour);
    for (uint i = 0; i < pWidth - threshold - 1; ++i) {
      std::print(" ");
    }
    std::print("\x1b[49m");
  }
  else {
    std::print("\x1b[49;{}m", pPrimaryEscapeColour);
  }
  std::println("\x1b[39m {}%", ceil(pProgress*100)); // Reset
}

void printProgressBarUntilDone(std::mutex* pSTDOUTMutex, const std::function<float()>& pGetProgressFunc, const std::string& pTitle, uint8_t pPrimaryEscapeColour, uint8_t pSecondaryEscapeColour, uint pWidth) {
  float p = 0.f;
  while (p < 1) {
    p = pGetProgressFunc();
    {
      std::lock_guard<std::mutex> lock(*pSTDOUTMutex);
      printProgressBar(p, pTitle, pPrimaryEscapeColour, pSecondaryEscapeColour, pWidth);
    }
    if (p < 1)
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
}

std::future<void> startProgressBar(std::mutex* pSTDOUTMutex, const std::function<float()>& pGetProgressFunc, const std::string& pTitle, uint8_t pPrimaryEscapeColour, uint8_t pSecondaryEscapeColour, uint pWidth) {
  return std::async(std::launch::async, printProgressBarUntilDone, pSTDOUTMutex, pGetProgressFunc, pTitle, pPrimaryEscapeColour, pSecondaryEscapeColour, pWidth);
}

