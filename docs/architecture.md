# Architecture

## Layers

```text
ui          window, renderer, controls, pages
app         application wiring, view state, strings
minecraft   version model, catalog, install and launch services
download    winhttp transfers, resume metadata
platform    package manager, activation, registry, processes, shell, dpi
core        errors, logging, settings, json, paths, task scheduling
```

Dependencies only point downward. The interface never touches the file
system, the registry or the package manager directly; it calls methods on
`Application`, which forwards to `MinecraftService`.

## State flow

```text
control click
  -> Application command
  -> MinecraftService (starts work on a worker)
  -> worker updates the service snapshot and posts a change notice
  -> Application rebuilds AppState from the snapshot on the window thread
  -> pages update their controls, the window repaints
```

`MinecraftService::snapshot()` is the only shared data structure. Workers
lock it, mutate it, and post a coalesced notification through `Dispatcher`,
which wakes the window thread with a posted message.

## Threads

- The window thread owns the HWND, the renderer and every element.
- `TaskScheduler` runs short jobs on a pool of two workers and gives long
  operations (downloads, deployments, launches) their own `std::jthread` with
  a stop token for cancellation.
- Workers never touch elements. Results always cross through `Dispatcher`.
- Downloads abort blocking WinHTTP reads by closing the request handle from
  the stop callback.

## Rendering

```text
HWND (WS_EX_NOREDIRECTIONBITMAP)
  -> D3D11 device
  -> DXGI flip swap chain created for composition
  -> Direct2D device context targeting the back buffer
  -> DirectWrite with an in-memory font collection (Hanken Grotesk, Schibsted Grotesk, JetBrains Mono)
  -> DirectComposition visual bound to the window
```

Rendering is event driven. A frame is drawn on WM_PAINT and when an element
invalidates. Animations request single frames through a 16 ms timer that is
stopped as soon as nothing animates. Idle cost is zero frames.

Layout works in device independent pixels. The window converts mouse
coordinates and client sizes with the window DPI, the renderer sets the same
DPI on the Direct2D context, and WM_DPICHANGED re-layouts everything.

## Window frame

The window keeps `WS_OVERLAPPEDWINDOW` so minimize, maximize, snapping and
the system menu behave normally. `WM_NCCALCSIZE` removes the standard frame,
`WM_NCHITTEST` maps the resize borders, the caption drag area and the
maximize button (`HTMAXBUTTON`, which enables snap layouts), and DWM draws
the rounded corners and the shadow.

## Elements

`Element` is a small retained tree: measure, arrange, render, hit test and
input callbacks. Pages are elements that rebuild their dynamic rows when the
set of versions changes and update labels in place otherwise, so focus and
scroll positions survive state updates. When an element is destroyed it
tells the window so no hover, pressed or focus pointer can dangle.

## Errors

Every operation returns `Result<T>` (`std::expected<T, Error>`). An `Error`
carries a category, the operation, a human readable message, technical
detail, an HRESULT or Win32 code and whether a retry makes sense. The
interface shows the message in a toast and the detail behind a Details
button.

## Startup

```text
settings loaded          ~10 ms
window and graphics      ~250 ms
first frame              ~290 ms
then, deferred:
  installed package scan, catalog refresh, environment check, update check
```

Timings are written to the log on every start.
