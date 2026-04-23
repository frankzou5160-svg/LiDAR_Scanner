import serial
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import numpy as np
import threading
import queue
import sys

# ── CONFIG ──────────────────────────────────────────────────────────────────
PORT      = 'COM3'       # change to your STM32 COM port
BAUDRATE  = 115200
# ─────────────────────────────────────────────────────────────────────────────

data_queue = queue.Queue()
current_sweep  = []   # points building up this sweep  [(angle, dist), ...]
previous_sweep = []   # last completed sweep

def serial_reader():
    """Runs in background thread — reads lines and pushes to queue."""
    try:
        ser = serial.Serial(PORT, BAUDRATE, timeout=1)
        print(f"Connected to {PORT} at {BAUDRATE} baud")
    except serial.SerialException as e:
        print(f"ERROR: Could not open {PORT}: {e}")
        print("Check your COM port number in the script (PORT = 'COM3')")
        sys.exit(1)

    while True:
        try:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if line:
                data_queue.put(line)
        except Exception:
            pass

def parse_line(line):
    """
    Returns:
        ('data',  angle_deg, dist_cm)  for A###,### lines
        ('sweep', None,      None)     for A360
        (None,    None,      None)     on parse error
    """
    if not line.startswith('A'):
        return None, None, None

    if line == 'A360':
        return 'sweep', None, None

    try:
        parts = line[1:].split(',')
        angle = int(parts[0])
        dist  = int(parts[1])
        return 'data', angle, dist
    except (IndexError, ValueError):
        return None, None, None

def to_xy(angle_deg, dist_cm):
    rad = np.radians(angle_deg)
    x = dist_cm * np.cos(rad)
    y = dist_cm * np.sin(rad)
    return x, y

# ── PLOT SETUP ───────────────────────────────────────────────────────────────
fig, (ax_polar, ax_cart) = plt.subplots(1, 2, figsize=(14, 7),
                                         facecolor='#0a0a0f')
fig.suptitle('STM32 LiDAR Scanner', color='#00ff88',
             fontsize=14, fontweight='bold', fontfamily='monospace')

# Polar axes
ax_polar = plt.subplot(121, projection='polar', facecolor='#0d0d1a')
ax_polar.set_title('Live Sweep', color='#00ff88',
                   fontfamily='monospace', pad=15)
ax_polar.tick_params(colors='#444466')
ax_polar.spines['polar'].set_color('#222244')
ax_polar.set_facecolor('#0d0d1a')
for line in ax_polar.xaxis.get_gridlines() + ax_polar.yaxis.get_gridlines():
    line.set_color('#1a1a2e')

# Cartesian axes
ax_cart = plt.subplot(122, facecolor='#0d0d1a')
ax_cart.set_title('Completed Sweep Map', color='#00ff88',
                  fontfamily='monospace')
ax_cart.set_facecolor('#0d0d1a')
ax_cart.tick_params(colors='#444466')
ax_cart.spines[:].set_color('#222244')
ax_cart.set_aspect('equal')
ax_cart.grid(True, color='#1a1a2e', linewidth=0.5)
ax_cart.axhline(0, color='#222244', linewidth=0.8)
ax_cart.axvline(0, color='#222244', linewidth=0.8)

# Scanner origin marker
ax_cart.plot(0, 0, 'o', color='#00ff88', markersize=6, zorder=5)

# Status text
status_text = fig.text(0.5, 0.02, 'Waiting for data...', ha='center',
                       color='#666688', fontfamily='monospace', fontsize=9)

plt.tight_layout(rect=[0, 0.04, 1, 0.96])

def update(frame):
    global current_sweep, previous_sweep

    # drain the queue
    new_points = []
    sweep_done = False
    while not data_queue.empty():
        try:
            line = data_queue.get_nowait()
        except queue.Empty:
            break

        kind, angle, dist = parse_line(line)

        if kind == 'sweep':
            sweep_done = True
        elif kind == 'data' and dist > 0:   # dist == 0 means invalid
            new_points.append((angle, dist))

    current_sweep.extend(new_points)

    if sweep_done and current_sweep:
        previous_sweep = list(current_sweep)
        current_sweep  = []

    # ── POLAR PLOT (live current sweep) ──────────────────────────────────────
    ax_polar.cla()
    ax_polar.set_facecolor('#0d0d1a')
    ax_polar.set_title('Live Sweep', color='#00ff88',
                       fontfamily='monospace', pad=15)
    ax_polar.tick_params(colors='#444466')
    for line in ax_polar.xaxis.get_gridlines() + ax_polar.yaxis.get_gridlines():
        line.set_color('#1a1a2e')

    if previous_sweep:
        prev_r   = [d for _, d in previous_sweep]
        prev_ang = [np.radians(a) for a, _ in previous_sweep]
        ax_polar.scatter(prev_ang, prev_r, s=1.5, c='#223344', alpha=0.5)

    if current_sweep:
        curr_r   = [d for _, d in current_sweep]
        curr_ang = [np.radians(a) for a, _ in current_sweep]
        ax_polar.scatter(curr_ang, curr_r, s=2, c='#00ff88', alpha=0.9)

    # ── CARTESIAN PLOT (last completed sweep) ────────────────────────────────
    ax_cart.cla()
    ax_cart.set_facecolor('#0d0d1a')
    ax_cart.set_title('Completed Sweep Map', color='#00ff88',
                      fontfamily='monospace')
    ax_cart.tick_params(colors='#444466')
    ax_cart.spines[:].set_color('#222244')
    ax_cart.set_aspect('equal')
    ax_cart.grid(True, color='#1a1a2e', linewidth=0.5)
    ax_cart.axhline(0, color='#222244', linewidth=0.8)
    ax_cart.axvline(0, color='#222244', linewidth=0.8)
    ax_cart.plot(0, 0, 'o', color='#00ff88', markersize=6, zorder=5)

    if previous_sweep:
        xs = [to_xy(a, d)[0] for a, d in previous_sweep]
        ys = [to_xy(a, d)[1] for a, d in previous_sweep]
        ax_cart.scatter(xs, ys, s=3, c='#00ccff', alpha=0.85)

        # distance rings for reference
        max_d = max(d for _, d in previous_sweep)
        for r in range(50, int(max_d) + 50, 50):
            circle = plt.Circle((0, 0), r, fill=False,
                                 color='#1a1a2e', linewidth=0.6)
            ax_cart.add_patch(circle)
            ax_cart.text(r, 2, f'{r}cm', color='#333355',
                         fontsize=7, fontfamily='monospace')

    # status bar
    total = len(previous_sweep)
    building = len(current_sweep)
    status_text.set_text(
        f'Last sweep: {total} points    |    Building: {building} points'
    )

# ── START ─────────────────────────────────────────────────────────────────────
t = threading.Thread(target=serial_reader, daemon=True)
t.start()

ani = animation.FuncAnimation(fig, update, interval=100, cache_frame_data=False)

plt.show()