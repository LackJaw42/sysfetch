# sysfetch

A lightweight Waybar custom module written in C that displays **CPU temperature**, **memory usage**, and **battery percentage** in your status bar.

```
CPU: 55°C | MEM: 42% | BAT: 87%
```

## Requirements

- Debian/Ubuntu Linux
- [Waybar](https://github.com/Alexays/Waybar)
- `gcc` and `cmake`

```bash
sudo apt install gcc cmake waybar
```

## Build & Install

```bash
# For SSH
git clone git@github.com:LackJaw42/sysfetch.git

# For HTTPS
https://github.com/LackJaw42/sysfetch.git

cd sysfetch
cmake -B build
cmake --build build
sudo cmake --install build
```

## Waybar Configuration

Add to `~/.config/waybar/config`:

```json
"custom/sysfetch": {
    "exec": "/usr/local/bin/sysfetch",
    "interval": 5,
    "return-type": "json",
    "format": "{text}",
    "tooltip": true
}
```

And include it in your module list:

```json
"modules-right": ["pulseaudio", "network", "custom/sysfetch"]
```

## Styling

Add to `~/.config/waybar/style.css`:

```css
#custom-sysfetch.ok       { color: #cdd6f4; }
#custom-sysfetch.warning  { color: #f9e2af; }
#custom-sysfetch.critical { color: #f38ba8; }
```

| Class | Condition |
|---|---|
| `ok` | Everything normal |
| `warning` | CPU ≥ 70°C, MEM ≥ 75%, BAT ≤ 20% |
| `critical` | CPU ≥ 85°C, MEM ≥ 90%, BAT ≤ 10% |

## License

MIT
