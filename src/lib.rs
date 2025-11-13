use std::{io::Cursor, rc::Rc, cell::RefCell, thread};
use rust_embed::RustEmbed;
use serde_json::json;
use tao::{event::{Event, WindowEvent}, event_loop::{ControlFlow, EventLoopBuilder, EventLoopProxy, EventLoopWindowTarget}, window::{Icon, Window, WindowBuilder}};
use tiny_http::{Header, Response, Server};
use tray_icon::{Icon as TrayIcon, TrayIconBuilder, menu::{Menu, MenuEvent, MenuItem}};
use wry::{WebViewBuilder, WebView};

mod funcs;
mod performance;
mod audio;
mod files;

#[derive(RustEmbed)]
#[folder = "flutter/build/web"]
struct Assets;

#[derive(Default)]
pub struct App {
    window: Option<Window>,
    webview: Option<WebView>,
}

pub enum ServerCommand {
    CreateWindow,
}

#[cfg(target_os = "windows")]
const ICON: &[u8; 105484] = include_bytes!("../icons/icon.ico");
#[cfg(target_os = "linux")]
const ICON: &[u8; 12672] = include_bytes!("../icons/icon.png");

fn handle_ipc(cmd: &str, args: serde_json::Value) -> serde_json::Value {
    if cmd == "get_soundboard" {
        let soundboard = funcs::get_soundboard();
        return json!({"result": soundboard});
    } else if cmd == "get_channels" {
        let channels = funcs::get_channels();
        return json!({"result": channels});
    } else if cmd == "new_channel" {
        if let Some(col) = args.get("color").and_then(|v| v.as_array()) {
            let mut color: [u8; 3] = [0, 0, 0];
            for (i, val) in col.iter().enumerate() {
                color[i] = val.as_i64().unwrap_or_default() as u8;
            }
            if let Some(icon) = args.get("icon").and_then(|v| v.as_str()) {
                if let Some(name) = args.get("name").and_then(|v| v.as_str()) {
                    if let Some(deviceapps) = args.get("deviceapps").and_then(|v| v.as_str()) {
                        if let Some(device) = args.get("device").and_then(|v| v.as_bool()) {
                            if let Some(low) = args.get("low").and_then(|v| v.as_bool()) {
                                let res = funcs::new_channel(
                                    color,
                                    icon.to_string(),
                                    name.to_string(),
                                    deviceapps.to_string(),
                                    device,
                                    low,
                                );
                                return json!({"result": res});
                            }
                        }
                    }
                }
            }
        }
    } else if cmd == "new_sound" {
        if let Some(col) = args.get("color").and_then(|v| v.as_array()) {
            let mut color: [u8; 3] = [0, 0, 0];
            for (i, val) in col.iter().enumerate() {
                color[i] = val.as_i64().unwrap_or_default() as u8;
            }
            if let Some(icon) = args.get("icon").and_then(|v| v.as_str()) {
                if let Some(name) = args.get("name").and_then(|v| v.as_str()) {
                    if let Some(sound) = args.get("sound").and_then(|v| v.as_str()) {
                        if let Some(low) = args.get("low").and_then(|v| v.as_bool()) {
                            let res = funcs::new_sound(
                                color,
                                icon.to_string(),
                                name.to_string(),
                                sound.to_string(),
                                low,
                            );
                            return json!({"result": res});
                        }
                    }
                }
            }
        }
    } else if cmd == "edit_channel" {
        if let Some(col) = args.get("color").and_then(|v| v.as_array()) {
            let mut color: [u8; 3] = [0, 0, 0];
            for (i, val) in col.iter().enumerate() {
                color[i] = val.as_i64().unwrap_or_default() as u8;
            }
            if let Some(icon) = args.get("icon").and_then(|v| v.as_str()) {
                if let Some(name) = args.get("name").and_then(|v| v.as_str()) {
                    if let Some(deviceapps) = args.get("deviceapps").and_then(|v| v.as_str()) {
                        if let Some(device) = args.get("device").and_then(|v| v.as_bool()) {
                            if let Some(oldname) = args.get("oldname").and_then(|v| v.as_str()) {
                                if let Some(low) = args.get("low").and_then(|v| v.as_bool()) {
                                    let res = funcs::edit_channel(
                                        color,
                                        icon.to_string(),
                                        name.to_string(),
                                        deviceapps.to_string(),
                                        device,
                                        oldname.to_string(),
                                        low,
                                    );
                                    return json!({"result": res});
                                }
                            }
                        }
                    }
                }
            }
        }
    } else if cmd == "edit_soundboard" {
        if let Some(col) = args.get("color").and_then(|v| v.as_array()) {
            let mut color: [u8; 3] = [0, 0, 0];
            for (i, val) in col.iter().enumerate() {
                color[i] = val.as_i64().unwrap_or_default() as u8;
            }
            if let Some(icon) = args.get("icon").and_then(|v| v.as_str()) {
                if let Some(name) = args.get("name").and_then(|v| v.as_str()) {
                    if let Some(sound) = args.get("sound").and_then(|v| v.as_str()) {
                        if let Some(oldname) = args.get("oldname").and_then(|v| v.as_str()) {
                            if let Some(low) = args.get("low").and_then(|v| v.as_bool()) {
                                let res = funcs::edit_soundboard(
                                    color,
                                    icon.to_string(),
                                    name.to_string(),
                                    sound.to_string(),
                                    oldname.to_string(),
                                    low,
                                );
                                return json!({"result": res});
                            }
                        }
                    }
                }
            }
        }
    } else if cmd == "delete_channel" {
        if let Some(name) = args.get("name").and_then(|v| v.as_str()) {
            let res = funcs::delete_channel(name.to_string());
            return json!({"result": res});
        }
    } else if cmd == "delete_sound" {
        if let Some(name) = args.get("name").and_then(|v| v.as_str()) {
            let res = funcs::delete_sound(name.to_string());
            return json!({"result": res});
        }
    } else if cmd == "pick_menu_sound" {
        let sound = funcs::pick_menu_sound();
        return json!({"result": sound});
    } else if cmd == "get_devices" {
        let devices = funcs::get_devices();
        return json!({"result": devices});
    } else if cmd == "get_apps" {
        let apps = funcs::get_apps();
        return json!({"result": apps});
    } else if cmd == "save_settings" {
        if let Some(output) = args.get("output").and_then(|v| v.as_str()) {
            if let Some(scale) = args.get("scale").and_then(|v| v.as_f64()) {
                if let Some(light) = args.get("light").and_then(|v| v.as_bool()) {
                    if let Some(monitor) = args.get("monitor").and_then(|v| v.as_bool()) {
                        if let Some(peaks) = args.get("peaks").and_then(|v| v.as_bool()) {
                            let res = funcs::save_settings(
                                output.to_string(),
                                scale as f32,
                                light,
                                monitor,
                                peaks,
                            );
                            return json!({"result": res});
                        }
                    }
                }
            }
        }
    } else if cmd == "get_performance" {
        let performance = funcs::get_performance();
        return json!({"result": performance});
    } else if cmd == "clear_performance" {
        funcs::clear_performance();
    } else if cmd == "get_settings" {
        let settings = funcs::get_settings();
        return json!({"result": settings});
    } else if cmd == "set_volume" {
        if let Some(name) = args.get("name").and_then(|v| v.as_str()) {
            if let Some(volume) = args.get("volume").and_then(|v| v.as_f64()) {
                funcs::set_volume(name.to_string(), volume as f32);
            }
        }
    } else if cmd == "get_outputs" {
        let outputs = funcs::get_outputs();
        return json!({"result": outputs});
    } else if cmd == "play_sound" {
        if let Some(sound) = args.get("sound").and_then(|v| v.as_str()) {
            if let Some(low) = args.get("low").and_then(|v| v.as_bool()) {
                funcs::play_sound(sound.to_string(), low);
            }
        }
    } else if cmd == "get_volume" {
        if let Some(name) = args.get("name").and_then(|v| v.as_str()) {
            if let Some(get) = args.get("get").and_then(|v| v.as_bool()) {
                if let Some(device) = args.get("device").and_then(|v| v.as_bool()) {
                    let volume = funcs::get_volume(name.to_string(), get, device);
                    return json!({"result": volume});
                }
            }
        }
    } else if cmd == "uninstall" {
        let res = funcs::uninstall();
        return json!({"result": res});
    } else if cmd == "update" {
        let res = funcs::update();
        return json!({"result": res});
    } else if cmd == "flutter_print" {
        if let Some(text) = args.get("text").and_then(|v| v.as_str()) {
            println!("{}", text);
        }
    } else if cmd == "get_version" {
        let version = env!("CARGO_PKG_VERSION");
        return json!({ "result": version });
    } else if cmd == "open_link" {
        if let Some(link) = args.get("link").and_then(|v| v.as_str()) {
            return json!({"result": open::that(link).map_err(|e| e.to_string())});
        }
    }

    serde_json::Value::Null
}

pub fn create_icon() -> Option<(Vec<u8>, u32, u32)> {
    #[cfg(target_os = "windows")]
    {
        let reader = Cursor::new(ICON);
        let icon_dir = match ico::IconDir::read(reader) {
            Ok(i) => i,
            Err(e) => {
                println!("Error occured when getting ico: {:#?}", e);
                return None;
            }
        };

        let entry = icon_dir
            .entries()
            .iter()
            .max_by_key(|e| e.width());
        
        let image = match entry {
            Some(i) => i,
            None => {
                println!("Failed to get an entry in ico");
                return None;
            }
        };

        match image.decode() {
            Ok(i) => {
                return Some((i.rgba_data().to_vec(), i.width(), i.height()));
            }
            Err(e) => {
                println!("Failed to decode icon: {:#?}", e);
                return None;
            }
        };
    }

    #[cfg(target_os = "linux")]
    {
        let reader = Cursor::new(ICON);

        let decoder = png::Decoder::new(reader);
        let mut decode = match decoder.read_info() {
            Ok(r) => r,
            Err(e) => {
                println!("Failed to read PNG: {:#?}", e);
                return None;
            }
        };

        let buffer_size = match decode.output_buffer_size() {
            Some(sz) => sz,
            None => {
                println!("Could not determine PNG output buffer size");
                return None;
            }
        };

        let mut buf = vec![0; buffer_size];
        let info = match decode.next_frame(&mut buf) {
            Ok(info) => info,
            Err(e) => {
                println!("Failed to decode PNG: {:#?}", e);
                return None;
            }
        };

        buf.truncate(info.buffer_size());
        Some((buf, info.width, info.height))
    }
}

pub fn run_server(ready_tx: &std::sync::mpsc::Sender<()>, proxy: EventLoopProxy<ServerCommand>) {
    let server = match Server::http("127.0.0.1:5923") {
        Ok(s) => s,
        Err(e) => {
            eprintln!("Failed to bind server to 127.0.0.1:5923: {}", e);
            let _ = ready_tx.send(());
            return;
        }
    };
    println!("Server listening on 127.0.0.1:5923");
    
    ready_tx.send(()).unwrap();

    for mut request in server.incoming_requests() {
        if request.method() == &tiny_http::Method::Post && request.url() == "/ipc" {
            let mut body = String::new();
            if let Err(e) = request.as_reader().read_to_string(&mut body) {
                let _ = request.respond(
                    Response::from_string(format!("Failed to read request body: {}", e)).with_status_code(400)
                );
                continue;
            }

            match serde_json::from_str::<serde_json::Value>(&body) {
                Ok(val) => {
                    let cmd = val.get("cmd").and_then(|v| v.as_str());
                    let args = val.get("args").cloned().unwrap_or(serde_json::Value::Null);

                    if let Some(cmd_str) = cmd {
                        let result = handle_ipc(cmd_str, args);
                        let body = serde_json::to_string(&result).unwrap_or_else(|_| "null".to_string());
                        let mut response = Response::from_string(body);
                        response.add_header(Header::from_bytes(&b"Content-Type"[..], &b"application/json"[..]).unwrap());
                        let _ = request.respond(response);
                    } else {
                        let _ = request.respond(Response::from_string("Missing 'cmd' field").with_status_code(400));
                    }
                }
                Err(e) => {
                    let _ = request.respond(Response::from_string(format!("Invalid JSON: {}", e)).with_status_code(400));
                }
            }

            continue;
        } else if request.method() == &tiny_http::Method::Get && request.url() == "/webview" {
            let _ = proxy.send_event(ServerCommand::CreateWindow);
            let mut response = Response::from_string("Success");
            response.add_header(Header::from_bytes(&b"Content-Type"[..], &b"text/plain"[..]).unwrap());
            let _ = request.respond(response);
            continue;
        }

        let path = if request.url() == "/" { "index.html" } else { &request.url()[1..] };

        if let Some(file) = Assets::get(path) {
            let mime = match path.rsplit('.').next() {
                Some("js") => "application/javascript",
                Some("css") => "text/css",
                Some("html") => "text/html",
                Some("json") => "application/json",
                Some("wasm") => "application/wasm",
                Some("woff2") => "font/woff2",
                _ => "application/octet-stream",
            };
            let mut response = Response::from_data(file.data.into_owned());
            response.add_header(Header::from_bytes(&b"Content-Type"[..], mime.as_bytes()).unwrap());
            response.add_header(Header::from_bytes(&b"Cache-Control"[..], &b"no-store"[..]).unwrap());
            request.respond(response).unwrap();
        } else {
            if let Some(file) = Assets::get("index.html") {
                let mut response = Response::from_data(file.data.into_owned());
                response.add_header(Header::from_bytes(&b"Content-Type"[..], &b"text/html"[..]).unwrap());
                response.add_header(Header::from_bytes(&b"Cache-Control"[..], &b"no-store"[..]).unwrap());
                request.respond(response).unwrap();
            } else {
                request.respond(Response::from_string("Not Found").with_status_code(404)).unwrap();
            }
        }
    }
}

pub fn create_window(event_loop: &EventLoopWindowTarget<ServerCommand>, app: &Rc<RefCell<App>>, hide_window: bool) {
    if app.borrow().window.is_none() {
        if !hide_window {
            let icon = create_icon().map(|(data, width, height)| Icon::from_rgba(data, width, height).unwrap());
            let window = WindowBuilder::new()
                .with_title("Vice")
                .with_window_icon(icon)
                .build(event_loop)
                .unwrap();

            app.try_borrow_mut().unwrap().window = Some(window);
        }
    }

    if app.borrow().webview.is_none() {
        if !hide_window {
            let webview = WebViewBuilder::new()
                .with_url("http://127.0.0.1:5923")
                .with_devtools(true)
                .build(&app.borrow().window.as_ref().unwrap())
                .unwrap();

            app.try_borrow_mut().unwrap().webview = Some(webview);
        }
    }
}

pub fn run() {
    if let Ok(client) = reqwest::blocking::Client::builder().timeout(std::time::Duration::from_millis(250)).build() {
        match client.get("http://127.0.0.1:5923").send() {
            Ok(_) => {
                println!("Calling http://127.0.0.1:5923/webview");
                let _ = client.get("http://127.0.0.1:5923/webview").send();
                std::process::exit(0);
            }
            Err(_) => {},
        }
    }

    files::create_files();
    audio::start();
    performance::start();

    let (tx, rx) = std::sync::mpsc::channel();
    let event_loop = EventLoopBuilder::<ServerCommand>::with_user_event().build();
    let proxy = event_loop.create_proxy();
    
    thread::spawn(move || {
        run_server(&tx, proxy);
    });
    rx.recv().unwrap();

    let args: Vec<String> = std::env::args().collect();
    let hide_window: bool = args.contains(&"--background".to_string());

    let app = Rc::new(RefCell::new(App::default()));
    create_window(&event_loop, &app, hide_window);

    let tray_icon = create_icon().map(|(data, width, height)| TrayIcon::from_rgba(data, width, height).unwrap())
        .unwrap_or(TrayIcon::from_rgba(vec![0, 0, 0, 0], 1, 1).unwrap());

    let tray_menu = Menu::new();
    let quit = MenuItem::new("Quit", true, None);
    let open_ui = MenuItem::new("Open UI", true, None);
    let restart = MenuItem::new("Restart Audio", true, None);
    tray_menu.append(&quit).unwrap();
    tray_menu.append(&open_ui).unwrap();
    tray_menu.append(&restart).unwrap();

    let _tray = TrayIconBuilder::new()
        .with_icon(tray_icon)
        .with_menu(Box::new(tray_menu))
        .with_tooltip("Vice")
        .with_title("Vice")
        .build()
        .expect("Failed to build tray icon");

    event_loop.run(move |event, event_loop_target, control_flow| {
        *control_flow = ControlFlow::Wait;

        match event {
            Event::WindowEvent {
                event: WindowEvent::CloseRequested,
                ..
            } => {
                app.try_borrow_mut().unwrap().webview = None;
                app.try_borrow_mut().unwrap().window = None;
            },
            Event::UserEvent(ServerCommand::CreateWindow) => {
                create_window(event_loop_target, &app, false);
            },
            _ => (),
        }

        if let Ok(menu_event) = MenuEvent::receiver().try_recv() {
            if menu_event.id() == quit.id() {
                std::process::exit(0);
            } else if menu_event.id() == open_ui.id() {
                create_window(event_loop_target, &app, false);
            } else if menu_event.id() == restart.id() {
                audio::restart();
            }
        }
    });
}