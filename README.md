# LocalTether

LocalTether is a versatile cross-platform client-server application designed for secure, real-time remote input sharing, file exchange, and communication. It features a graphical user interface built with ImGui and robust networking capabilities using ASIO with SSL/TLS encryption. The application is designed to work on both Windows and Linux systems.

## Features

*   **Secure Client-Server Communication**: All traffic between clients and the server is encrypted using SSL/TLS. The server can generate necessary SSL certificates automatically.
*   **Flexible Session Management**:
    *   **Roles**: Clients can connect as a **Host**, **Broadcaster**, or **Receiver**.
        *   The first client to connect with the "Host" intention typically becomes the session Host.
        *   The Host can manage other clients.
    *   **Password Protection**: Sessions can be protected with a password set by the Host.
    *   **Server Discovery**: Clients can scan the local network to find active LocalTether servers.
    *   **WAN Accessibility**: The server can be configured to accept connections from the internet (requires port forwarding) or be restricted to the local network only (default). Local network restriction allows connections from `127.0.0.1`, `192.168.x.x`, and `172.16.x.x` subnets.
*   **Remote Input Sharing**:
    *   A designated **Host** or **Broadcaster** client can stream keyboard and mouse input.
    *   **Receiver** clients can receive this input, enabling remote control or presentation scenarios.
    *   The Host can toggle input reception for individual Receiver clients.
*   **File Sharing & Synchronization**:
    *   A central `server_storage` directory (created in the project root or executable directory) acts as a shared file space.
    *   Clients can upload files to the server's shared storage.
    *   Clients can download files from the server.
    *   An integrated File Explorer panel in the UI provides a synchronized view of the shared files for all clients.
    *   Includes path traversal protection for file operations.
*   **Host Controls**:
    *   The Host can view a list of connected clients.
    *   Kick clients from the session.
    *   Rename clients.
    *   Toggle input stream reception for Receiver clients.
    *   Shutdown the server remotely.
*   **User-Friendly GUI**:
    *   Intuitive interface built with ImGui.
    *   Panels for:
        *   Initial setup (Hosting or Joining a session).
        *   Server configuration (WAN access, password).
        *   Client connection (server discovery, manual entry).
        *   A dashboard view when connected, including:
            *   **Console Panel**: Displays application logs and server messages.
            *   **File Explorer Panel**: For browsing and managing shared files.
            *   **Controls Panel**: For client management (Host) and input control.
*   **Logging**: Comprehensive logging for server and client activities.

## Prerequisites

*   **Operating System**: Windows or Linux.
*   **Windows**: PowerShell is required to run the `build.ps1` script.
*   **Linux**: A standard Linux environment with shell access.
*   **Git**: For cloning the repository.

All other dependencies (like CMake, C++ Compiler, vcpkg, and required libraries) are handled automatically by the respective build scripts.

## Getting Started

### 1. Download

Clone the repository to your local machine:

```bash
git clone https://github.com/abdurisaq/LocalTether
cd LocalTether
```

### 2. Build the Project

The project includes scripts to simplify the build process for both Windows and Linux. These scripts will download and set up all necessary dependencies and compile the application.

**For Linux:**

```bash
chmod +x scripts/build.sh
./scripts/build.sh
```

**For Windows:**
Open PowerShell (preferably as Administrator if you encounter permission issues for installing Chocolatey or vcpkg bootstrapping) and run:

```powershell
.\scripts\build.ps1
```

The build scripts will create a `build` directory in the project root and place the compiled executable inside it (e.g., `build/LocalTether` on Linux, `build/Release/LocalTether.exe` or `build/LocalTether.exe` on Windows).

### 3. Run the Application

**For Linux:**
The `run.sh` script typically performs a Debug build and then runs the executable.

```bash
chmod +x scripts/run.sh
./scripts/run.sh
```
Alternatively, you can run the executable directly from the `build` directory after running `build.sh`.

**For Windows:**
The `run.ps1` script can be used to launch the application after building. It attempts to find the built executable.

```powershell
.\scripts\run.ps1
```
Alternatively, navigate to the `build/Release` (or `build/Debug`) directory and run `LocalTether.exe` directly.

Logs will be output to `build/LocalTetherOutput.log` (Linux) or `scripts/build_log.txt` (Windows, for build process) and also displayed in the UI's Console Panel during runtime.

## Basic Usage

1.  **Hosting a Session**:
    *   Run the application.
    *   Click "Host New Session".
    *   Configure network settings:
        *   **Allow Internet Connections (WAN)**: Check this if you want clients outside your local network to connect (requires manual port forwarding on your router). If unchecked, the server only accepts connections from the local network.
        *   **Session Password**: Set an optional password for clients to join.
    *   Click "Start Hosting". The server will start, and an internal client for the host will connect automatically.
    *   You can now manage connected clients, share files, and broadcast input from the dashboard.

2.  **Joining a Session**:
    *   Run the application.
    *   Click "Join Existing Session".
    *   **Server Discovery**:
        *   Click "Scan For Servers" to find active LocalTether servers on your local network.
        *   Select a discovered server from the list to auto-fill the IP address.
    *   **Manual Connection**:
        *   Enter the Server IP Address.
        *   Enter the Session Password (if required).
        *   Enter Your Name.
    *   Click "Connect".
    *   Once connected, you can access shared files and receive input if you are a Receiver.

The `server_storage` directory, created in the project's root directory (e.g., `/path/to/LocalTether/server_storage`) or alongside the executable if the project root isn't found, is the shared space for files uploaded and downloaded through the application.