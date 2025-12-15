#pragma once

#include <print>
#include <future>
#include <thread>
#include <chrono>
#include <cmath>

void printProgressBar(float pProgress, const std::string& pTitle, uint8_t pPrimaryEscapeColour = 31, uint8_t pSecondaryEscapeColour = 30, uint pWidth = 20);

void printProgressBarUntilDone(std::mutex* pSTDOUTMutex, const std::function<float()>& pGetProgressFunc, const std::string& pTitle, uint8_t pPrimaryEscapeColour = 31, uint8_t pSecondaryEscapeColour = 30, uint pWidth = 20);

std::future<void> startProgressBar(std::mutex* pSTDOUTMutex, const std::function<float()>& pGetProgressFunc, const std::string& pTitle, uint8_t pPrimaryEscapeColour = 31, uint8_t pSecondaryEscapeColour = 30, uint pWidth = 20);

