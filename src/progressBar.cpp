#include "progressBar.hpp"

void printProgressBar(const std::string& pTitle, uint64_t* pCompletedCount, uint64_t pTotal, float pTotalInv, std::mutex* pSTDOUTMutex, uint8_t pPrimaryEscapeColour, uint8_t pSecondaryEscapeColour, uint pWidth) {
  std::lock_guard<std::mutex> lock(*pSTDOUTMutex);

  float progress = *pCompletedCount * pTotalInv;
  // std::print("\e[1F\e[2K{}: ", pTitle);
  // std::print("\e[2k{}: ", pTitle);
  std::print("\r\e[2K{}: ", pTitle);
  // std::print("\e 8\e 7{} ", pTitle);
  float threshold = std::ceil(progress * pWidth);

  // std::print("\e[u\e[s{} ", pTitle);
  // std::print("\r{} ", pTitle);
  std::print("\e[{}m\e[{}m", pPrimaryEscapeColour, pPrimaryEscapeColour + 10);

  for (uint i = 0; i < threshold; ++i)
    std::print(" ");

  if (threshold <= pWidth - 1) {
    std::print("\e[{}m\e[{}m", pSecondaryEscapeColour + 10, pSecondaryEscapeColour);

    for (uint i = 0; i < pWidth - threshold - 1; ++i)
      std::print(" ");

    std::print("\e[49m");
  }
  else std::print("\e[49;{}m", pPrimaryEscapeColour);

  // std::println("\e[39m {}%, [ {} / {} ]", std::ceil(progress*100), *pCompletedCount, pTotal); // Reset
  // std::print("\e[39m {}%, [ {} / {} ]\r", std::ceil(progress*100), *pCompletedCount, pTotal); // Reset
  std::print("\e[39m {}%, [ {} / {} ]", std::ceil(progress*100), *pCompletedCount, pTotal); // Reset

  std::cout << std::flush;
}

static bool checkIfDoneForTime(uint64_t* pCompletedCount, uint64_t pTotal, std::chrono::duration<double, std::milli> pDuration) {
  std::chrono::time_point<std::chrono::system_clock> startTime = std::chrono::system_clock::now();
  while (std::chrono::duration<double, std::milli>(std::chrono::system_clock::now() - startTime).count() < pDuration.count())
    if (*pCompletedCount == pTotal) return true;
  return false;
}

void printProgressBarUntilDone(std::mutex* pSTDOUTMutex, const std::string& pTitle, uint64_t* pCompletedCount, uint64_t pTotal, uint8_t pPrimaryEscapeColour, uint8_t pSecondaryEscapeColour, uint pWidth) {
  float totalInv = 1.f/pTotal;
  printProgressBar(pTitle, pCompletedCount, pTotal, totalInv, pSTDOUTMutex, pPrimaryEscapeColour, pSecondaryEscapeColour, pWidth);
  while (!checkIfDoneForTime(pCompletedCount, pTotal, std::chrono::milliseconds(1000)))
    printProgressBar(pTitle, pCompletedCount, pTotal, totalInv, pSTDOUTMutex, pPrimaryEscapeColour, pSecondaryEscapeColour, pWidth);
  printProgressBar(pTitle, pCompletedCount, pTotal, totalInv, pSTDOUTMutex, pPrimaryEscapeColour, pSecondaryEscapeColour, pWidth);
  std::lock_guard<std::mutex> lock(*pSTDOUTMutex);
  std::println("");
}

std::future<void> startProgressBar(std::mutex* pSTDOUTMutex, const std::string& pTitle, uint64_t* pCompletedCount, uint64_t pTotal, uint8_t pPrimaryEscapeColour, uint8_t pSecondaryEscapeColour, uint pWidth) {
  return std::async(std::launch::async, printProgressBarUntilDone, pSTDOUTMutex, pTitle, pCompletedCount, pTotal, pPrimaryEscapeColour, pSecondaryEscapeColour, pWidth);
}

