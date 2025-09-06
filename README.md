# SimuDine — Inter-Process Communication System

This project simulates a restaurant environment using **System V Inter-Process Communication (IPC)** in C.  
It models different restaurant roles — cook, waiter, and customer — and coordinates them using shared memory, semaphores, and message queues.

---

## 🔹 Features
- **Cook process**: Prepares food items and updates shared memory.
- **Waiter process**: Serves customers and handles order communication.
- **Customer process**: Places orders and interacts with the system.
- **Restaurant manager**: Coordinates multiple processes to ensure smooth operation.
- **System V IPC**: Demonstrates shared memory usage, semaphores for synchronization, and inter-process messaging.

---

## 🔹 Files in the Project
- `cook.c` — logic for cook process  
- `waiter.c` — logic for waiter process  
- `customer.c` — logic for customer process  
- `restaurant.c` — central coordination logic  
- `ipc_shared.h` — common IPC structures and definitions  
- `makefile` — build instructions  

---

## 🔹 How to Build & Run
```bash
# Compile the project
make

# Run the restaurant simulation
./restaurant
 
