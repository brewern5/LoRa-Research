---
marp: true
theme: default
paginate: true
---

<!-- _class: lead -->

# LoRa 

**Nate Brewer**

---

# Problem Domain

- LoRa(Long Range) has a low bandwidth capacity, which allows for long range, but small payloads. Cannot send large files effectively. 

---

# Proposed Solution

## Proposed Solution

- Understand the **limitations** of **LoRa** in **multiple** different **network settings**.
- Create a **highly distributed** system of **LoRa transmitters** and **Receivers**

---

# Architecture

## System Overview

**Key Components:**
- **ESP32 Micro Controller**: **Brains** and main component running software 
- **LoRa Tx/Rx**: Transmission and reception devices

---


# Sprint Structure - Sprint 1

## Goals
- Simple **LoRa P2P network**
- Understand LoRa's **Spreading Factor** and **Bandwidth**
  
## Tasks
1. Understand how to make a **simple LoRa network** with **complete payload reception** and **transmission**
2. Push it to it's limits - **understand** the **limitations** of the devices (Bandwidth, range)
3. Create **evaluation matrix** for **combinations** of **Spreading factor and bandwidth**. 


---

# Sprint Structure - Sprint 2

## Goals
- LoRa **Mesh network** creation
- **Parelleization** of **Transmission and reception**

## Tasks
1. Create a more **complex network** of **LoRa devices**
2. Understand **LoRa mesh limitations**. 
3. **Spreading Factor** and **Bandwidth evaluation** (For Mesh)
4. **Distribute** **transmission** and **reception** to begin parallelization of Tx and Rx
    

---

# Learning with AI

## AI Integration

1. **Serialization** and **Parallelization** in **wired connections**.
2. Learning C/C++ with AI for **effective** programming of micro-controllers 

---

<!-- _class: lead -->

# Questions?

Thank you for your attention!
