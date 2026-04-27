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

- **Infrastructure-less environments**, such as **disaster
zones** and **remote communities**, impose severe bandwidth and
power constraints that **prevent conventional systems** from supporting **human-scale voice communication**.

---

# Proposed Solution

- Create a **low-power**, **extendable**, **long range network** utilizing **LoRa**
  - *LoRa(Long Range Radio) has a low bandwidth capacity, which allows for long range, but small payloads. Cannot send large files effectively.*
- **Integrate an AI model** for **semantic compression** to **bypass LoRa's limitation** in payload capacity

---

# What I Learned

- How to **define a real-world problem**
- I learned a lot about the **limitations of LoRa** and it's capabilities
- **Digital communications**
- **C++ programming** 
- How to **write a paper**, more about the research "game"
- How to present my work in an easy to understand way

---

# What issues I Faced

- **Physical hardware complexity**
  - Had some IoT experience but no where near this complex
- **Managing** a device with **multiple different modes of communication** and how to **effectively manipulate data** between different components
- **Managing** all the **different parts of the project** - between the paper, the testing and the implementation

---

# What I discovered

- How I can work with AI to help effectively solve my problems
  - Utilize AI to back and forth plan implementation 
- How to use AI to organize my thoughts, questions, and codebases
  - Use project structure inside Gen-AI agents
- That I can effectively solve a complex problem 
- C based programming is 
  - The control is 

---

## How I used AI for coding

- Think about AGILE structure and being a mnager. AI is my subordinate
  - Create features and requirements and self document all. Send to AI, confirm all progress makes sense
  - Rinse and repeat
  - When in a good place, have AI create an integration plan and review, rinse and repeat **WITH SELF REVIEW** 
  - Then have AI follow the plan and test along the way

--- 

## How I used AI for writing

- I created a project inside my GenAI agent (Claude)
  - Pasted the current paper within the context alongside a more layman description
  - Would copy and paste sections of the paper, asking for a review and potential edit points
    - **ALWAYS REVIEW THE AI'S WORK**
  - Rinse and repeat

---

# Sprint 1

## Goals
- Simple **LoRa P2P network**
- Understand LoRa's **Spreading Factor** and **Bandwidth**
  
## Tasks
1. Understand how to make a **simple LoRa network** with **complete payload reception** and **transmission**
2. Push it to it's limits - **understand** the **limitations** of the devices (Bandwidth, range)
3. Create a **packet structure** and **fragmentation protocol**
4. **Implement SD card reader** for logging and **storing intended transmissio**n file

---

# Sprint 2
## Goals
- Finish Paper
- Add GPS/GNSS Device 
- Update to state machine for further testing
## Tasks
1. Make the device work as a **state management machine** rather than a single-responsibiity, making it more extendable
2. **Bug fixing** and **finalize logging**
3. Added **GPS module** for accurate positioning for testing
4. **Write and submit paper** with results
5. **Poster** Presentation


---

# Paper

- https://github.com/brewern5/LoRa-Research/blob/main/docs/paper/paper.pdf

---

# Demo 

- https://www.youtube.com/shorts/5_YekBIIFng 

---

# Future 

- Bit off more than I can chew with mesh network implementation
  - I have the  framework and extension ready to continue but other work got in the way
- Continue testing and write another paper using mesh 

---

<!-- _class: lead -->

# Questions?

Thank you for your attention!
