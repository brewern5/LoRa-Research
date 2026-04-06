---
marp: true
title: Team Weekly Report
paginate: true
size: 16:9
---


# Learning-With-AI
### Nate Brewer

---

# How I use AI for questions

- I ask an AI a question. (Usually now Claude - Sometimes ChatGPT)
  - If it is a techincal or design question, I provide a potential answer and see it's response.
    - Still practice critical thinking
    - Take AI's answer with a grain of salt.
  - If it is a question about a theory or known technology, I will just ask.
- If I do not understand something in the response, quote it, and ask directly
- If it is questions about a broader topic, I will create a project for both organization but more importantly, context.

---

# How I use AI for projects. pt. 1

- I always create a project folder inside of my chosen generative model. I will then provide a description of what I am trying to acomplish (With varying details- dependent on how much I know)
- The more I decide is true about the project, the more context I will add to the description.
- Once I am in a state of understanding, I will create a MD doc that is structured like a feature/requirements list with a description of what the project is to help provide context. 

---

# How I use AI for projects. pt. 2
- Have a conversataion with my "worker" agent about the MD document. Begin planning more nuanced details of the project without coding
  - What libs to use, code-structure, psuedo-code
- Once ready, begin the sprint implementation. Tell the worker to implement a feature and all of it's requirements. 
- Check and test the output code. Rinse and repeat

---

# How I use AI for projects. pt. 3

- Adding new features will always start with having a conversation with the "worker" agent about what the new feature is, what it needs to work, and how to implement it into the current repo. 
- After the conversation create an implementation plan based on the conversation and review
- Begin the implementation and test the new code

---

# C++

- Better memory management
- With power (manual memory manipultion) comes responisbility (***effective*** manual memory manipultion)
- Compared to Java, much easier to optimize, but harder to build with.
  - Java's JVM garbage collection lives on the heap. 
---

# C++  -  Stack storage

- Stack - memory managed automatically by the compiler 
  - Scope-based 
  - Allocate and destroy automatically
```c++
  int foo(int x) { // Store function params
    int y = 10;     // Store Scope Vars
    Point p{3, 4}   // Object stored in stack - the raw 
    return x + y    // Store return values 
  }           
```

- Java still uses the stack. Puts:
  - Primative types - Object references - Return addresses - Arguements

---

# C++  -  Heap storage

- Heap - memory that is limited by RAM (Random Access Memory)
- C++ heap memory is manually controlled, meaning any unused allocations need to be freed
  - You can create automatic management - but it must be configured 
- Slower than stack but way more storage

```c++
void foo() {
    int* p = new int(10);   // Pointer - stored on heap memory
    delete p;               // must free manually!
}
```

- Java uses Heap for it's automatic storage which the JVM automatically manages.

---

# C++ - Heap & Stack

- C++ automatically puts data types into the stack unless otherwise stated
- Stated through pointers 

```c++ 
void foo {
  Point p{3, 4};                        // stack
  Point* p = new Point{3, 4};           // heap — you said "new"
  auto p = std::make_unique<Point>(3,4); // heap — you said "make_unique"
  auto p = std::make_shared<Point>(3,4); // heap — you said "make_shared"
  std::vector<Point> pts;               // vector itself on stack,
                                        // its internal buffer on heap
}
```

- Often memory leaks are obejcts that have been stored in heaps and not destroyed.

---


# Questions?