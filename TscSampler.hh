#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <cassert>
#include <iostream>

class TscSampler
{
public:
   TscSampler(const char* s = nullptr)
   {
      Start(s);
   }

   ~TscSampler()
   {
      if (m_s)
         Stop();
   }

   void Start(const char* s)
   {
      m_s = s;
      m_stamp = __rdtsc();
   }
   void Mark(const char* s)
   {
      uint64_t t = __rdtsc();
      assert(m_s);
      auto& p = s_samples[m_s];
      p.first += (t - m_stamp);
      p.second += 1;
      
      m_s = s;
      m_stamp = t;
   }
   
   void Stop()
   {
      assert(m_s);
      uint64_t s = __rdtsc();
      auto& p = s_samples[m_s];
      p.first += (s - m_stamp);
      p.second += 1;
      m_s = nullptr;
   }

   static void Dump()
   {
      for (auto& kv: s_samples)
      {
         std::cerr << kv.first << ": " << kv.second.first / kv.second.second << '\n';
      }
   }

private:
   const char* m_s;
   uint64_t m_stamp = 0;

   static std::map<std::string, std::pair<uint64_t, uint64_t>> s_samples;
};

std::map<std::string, std::pair<uint64_t, uint64_t>> TscSampler::s_samples;
