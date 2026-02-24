---
marp: true
title: Team Weekly Report
paginate: true
size: 16:9
---


# Week 6  Update 

## LoRa

**Nate Brewer**

---

# Acomplishment

- Created a half-duplex peer-2-peer interface with both 915MHz and 433MHz LoRa devices. 
- Understand and preparing testing for understand Spreading Factor and bandwidth.

---

# Unknowns
## Known Unknowns
 - How to use and program LoRa devices (Via ESP32)
 - How to create a half-duplex LoRa network
 - Antenna regulations (different power regulations)

## Unknown Unknowns 
 - The regulatory statues on LoRa transmission
   - Duty Cycle limitations
   - 433MHz vs 915MHz frequency 

---

# Encountered Challenges
- Combination of incompatble 915MHz devices with 433MHz devices

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

## Parallelization
    - Data sent as multiple bits through multiple lines
---

# Next steps
- Create testing simulation 
  - Add SD card reader for local logging
  - Add GPS/GNSS module for accurate positioning
- Begin Testing


---

# Questions?
