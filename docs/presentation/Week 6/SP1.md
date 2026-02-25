---
marp: true
title: Team Weekly Report
paginate: true
size: 16:9
---


# SP1 Presentation 

## LoRa

**Nate Brewer**

---

# Acomplishment

- Created a half-duplex peer-2-peer interface on 915MHz LoRa devices. 
- Understand and preparing testing for understand Spreading Factor and bandwidth.
- Packet structure and protocol

---

# Encountered Unknowns
## Known Unknowns
 - How to use and program LoRa devices (Via ESP32)
 - How to create a half-duplex LoRa network
 - How to structure LoRa packets
 - Antenna regulations (different power regulations)

## Unknown Unknowns 
 - The regulatory statues on LoRa transmission
   - Duty Cycle limitations
   - 433MHz vs 915MHz frequency 

---

# Encountered Challenges
- Combination of incompatble 915MHz devices with 433MHz devices
- Managing multiple files in Arduino IDE

--- 

# Learning With AI pt.1

 - **Being Direct and explicit matters**
   - Don't query *"Tell me about LoRa FCC regulations"*
     - Too broad - it assumes only the 915MHz device
 - ***With* who you chat with and *what* you query is**
   - As Cho said, the AI matters
   - ***ChatGPT*** 
     - more generic, good introduction to info 
     - Feels like a ***people pleaser***
   - ***Claude*** 
     - more techinical and direct 

---
# Learning with AI pt. 2

## Serialization
    - Data converted to byte sequences
      - Sent sequentially through *usually* single line
    - Most used today

## Parallelization
    - Data sent as multiple bits through multiple lines
    - More cables = more expensive but also faster
---
# Learning with AI pt.3 

## C++ pointers and references
- Reference
  - Avoids creating a copy of an object rather a reference to the original
  - More permanent
- Pointer 
  - Stores the memory address of another variable
    - Great for data structure iteration
    - More variable, can be reassigned
## Header files
- Storage declarations of functions, classes, and structures 

---
# Next steps
- Create testing simulation 
  - Send test compressed audio file and compare to raw audio
  - Add GPS/GNSS module for accurate positioning
- Next sprint, start creating a LoRa mesh network.

## Learning with AI - Next
- C++ memory management on memory limited devices
- Serialization vs. Parallelization in network comms

---

# Questions?
