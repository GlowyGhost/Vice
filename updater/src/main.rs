use crossterm::{
    event::{self, Event},
    terminal
};
use std::{
    io::{stdout, Write},
    thread,
    time::Duration,
    fs::{self, File},
    path::Path,
    process::{Command, Stdio},
    net::TcpStream,
    ffi::CString
};
use reqwest::blocking::Client;
use serde::Deserialize;
use zip::ZipArchive;
use winapi::um::winbase::{MoveFileExA, MOVEFILE_DELAY_UNTIL_REBOOT};

fn presskeytoquit() -> Result<(), Box<dyn std::error::Error>> {
    print!("Press any key to exit. This window will close in 10 seconds.");
    stdout().flush()?;

    let mut seconds = 10;
    let mut elapsed_ms = 0;
    let tick_ms = 100;

    while event::poll(Duration::from_millis(0))? {
        let _ = event::read()?;
    }
    loop {
        if event::poll(Duration::from_millis(0))? {
            if let Event::Key(_) = event::read()? {
                break;
            }
        }

        if elapsed_ms % 1000 == 0 {
            print!("\rPress any key to exit. This window will close in {} seconds.    ", seconds);
            stdout().flush()?;
            seconds -= 1;

            if seconds < 0 {
                println!();
                println!();
                break;
            }
        }

        thread::sleep(Duration::from_millis(tick_ms));
        elapsed_ms += tick_ms;
    }

    Ok(())
}

fn delete_folder(dir: String) -> Result<(), Box<dyn std::error::Error>> {
    let status = Command::new("cmd")
        .args(["/C", "rmdir", "/S", "/Q", &dir])
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .status()?;

    if !status.success() {
        return Err(format!("Failed to delete folder {}", dir).into());
    }

    Ok(())
}
fn delete_file(path: String) -> Result<(), Box<dyn std::error::Error>> {
    let file_path = Path::new(&path);
    if !file_path.exists() {
        return Err("File does not exist.".into());
    }
    if !is_executable(file_path) {
        return Err("File is not an executable.".into());
    }
    return Ok(fs::remove_file(file_path)?);
}
fn is_executable(path: &Path) -> bool {
    path.extension()
        .map_or(false, |ext| ext.eq_ignore_ascii_case("exe"))
}
fn close_vice(path: String) -> Result<(), Box<dyn std::error::Error>> {
    let process_name = Path::new(&path)
        .file_name()
        .and_then(|n| n.to_str())
        .unwrap_or(&path);

    let output = Command::new("tasklist")
        .arg("/FI")
        .arg(format!("IMAGENAME eq {}", process_name))
        .output()?;

    let stdout = String::from_utf8_lossy(&output.stdout).to_string();
    if !stdout.contains(process_name) {
        return Ok(());
    }

    let process_name = Path::new(&path)
        .file_name()
        .and_then(|n| n.to_str())
        .unwrap_or(&path);

    Command::new("taskkill")
        .args(["/F", "/T", "/IM", process_name])
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .spawn()?;

    Ok(())
}
fn schedule_delete(path: &str) {
    let c_path = CString::new(path).unwrap();
    unsafe {
        MoveFileExA(
            c_path.as_ptr(),
            std::ptr::null(),
            MOVEFILE_DELAY_UNTIL_REBOOT,
        );
    }
}
fn fix_permissions(path: &str) {
    let _ = Command::new("cmd")
        .args(["/C", "takeown", "/F", path, "/R", "/D", "Y"])
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .spawn();

    let _ = Command::new("cmd")
        .args(["/C", "icacls", path, "/grant", "Administrators:F", "/T", "/C"])
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .spawn();
}
fn kill_webview2_processes() {
    let processes = [
        "msedgewebview2.exe",
        "WebView2.exe",
        "WebView2Runtime.exe",
    ];

    for p in processes {
        let _ = Command::new("taskkill")
            .args(["/F", "/IM", p])
            .stdout(Stdio::null())
            .stderr(Stdio::null())
            .spawn();
    }
}
fn install_update(old_path: String, debug: String) -> Result<(), Box<dyn std::error::Error>> {
    print!("\rPreparing to download...");
    let client = Client::new();
    let release: Release = client
        .get("https://api.github.com/repos/Glowwy-Dev/Vice/releases/latest")
        .header("User-Agent", "Vice-Updater")
        .send()?
        .json()?;

    let target_asset = release.assets.iter().find(|a| a.name.ends_with("windows.zip"));

    let target_asset = match target_asset {
        Some(asset) => asset,
        None => {
            print!("\rNo suitable release asset found.");
            return Err("None".into());
        }
    };

    print!("\rDownloading {}", target_asset.name);
    let resp = client.get(&target_asset.browser_download_url).send()?.bytes()?;

    let tmp_zip = Path::new("update_tmp.zip");
    fs::write(&tmp_zip, &resp)?;

    print!("\rExtracting...                     ");

    let file = File::open(&tmp_zip)?;
    let mut archive = ZipArchive::new(file)?;

    let extract_dir = Path::new("update_tmp");
    if extract_dir.exists() {
        fs::remove_dir_all(extract_dir)?;
    }
    fs::create_dir_all(extract_dir)?;

    let expected_name = if debug == "false" { "Vice.exe" } else { "Vice-debug.exe" };

    let mut found = false;

    for i in 0..archive.len() {
        let mut file = archive.by_index(i)?;
        let name = Path::new(file.name())
            .file_name()
            .and_then(|n| n.to_str())
            .unwrap_or("");

        if name == expected_name {
            let mut outfile = File::create(&extract_dir.join(name))?;
            std::io::copy(&mut file, &mut outfile)?;
            found = true;
            break;
        }
    }

    if !found {
        print!("\rBinary '{}' not found in archive.           ", expected_name);
        fs::remove_file(tmp_zip)?;
        fs::remove_dir_all(extract_dir)?;
        return Err("None".into());
    }

    let new_binary = extract_dir.join(expected_name);
    fs::copy(&new_binary, &old_path)?;
    fs::remove_file(tmp_zip)?;
    fs::remove_dir_all(extract_dir)?;

    Ok(())
}

#[derive(Deserialize, Debug)]
struct Release {
    assets: Vec<Asset>,
}

#[derive(Deserialize, Debug)]
struct Asset {
    name: String,
    browser_download_url: String,
}

fn update(old_path: String, debug: String) -> Result<(), Box<dyn std::error::Error>> {
    print!("Verifying Internet Connection...");
    stdout().flush()?;

    let connected = TcpStream::connect_timeout(
        &("1.1.1.1:80".parse().unwrap()),
        Duration::from_secs(2)
    ).is_ok();

    if !connected {
        print!("\rNo Internet Connection.            ");
        println!();
        println!();

        println!("Update Failed. Internet connection is required to update.");
        println!("Please check your internet connection and try again.");
        return presskeytoquit();
    }

    print!("\rInternet Connection Verified.");
    println!();
    println!();

    println!("Please do not unplug your device or disconnect from the internet during the update process.");
    println!();
    println!();

    stdout().flush()?;

    //Close Vice if running
    stdout().flush()?;
    print!("Closing Vice...");

    let _ = close_vice(old_path.clone());

    print!("\rClosed Vice...         ");
    stdout().flush()?;
    println!();
    println!();

    let mut errored = false;

    //cache deletion
    stdout().flush()?;
    println!();

    print!("Deleting WebView data...");

    let path_cache = std::env::var("APPDATA").unwrap_or_default() + "\\Vice\\Cache";

    kill_webview2_processes();
    thread::sleep(Duration::from_millis(500));
    fix_permissions(&path_cache.clone());
    let res_com = delete_folder(path_cache.clone());

    match res_com {
        Ok(_) => {
            print!("\rDeleted WebView data...     ");
        }
        Err(e) => {
            if e.to_string().contains("Directory does not exist") {
                print!("\rDirectory \"{}\" does not exist. It's been deleted already.", path_cache);
            } else {
                print!("\rFailed to delete folder \"{}\": {}", path_cache, e);
                errored = true;
            }
        }
    }
    println!();
    println!();
    stdout().flush()?;

    //Download Update
    match install_update(old_path.clone(), debug) {
        Ok(_) => {
            print!("\rDownloaded Update...        ");
        }
        Err(e) => {
            if e.to_string() != "None" {
                print!("\rFailed to download update: {}", e);
            }
            
            println!();
            println!("You'll need to update app file manually.");
            println!("Other folders will be handled manually unless errored.");

            errored = true;
        }
    }
    println!();
    stdout().flush()?;

    print!("Opening new version...");
    let escaped = old_path.replace("&", "^&");

    let _ = Command::new("cmd")
        .args(["/C", "start", "", &escaped])
        .spawn();

    print!("\rOpened new version...      ");
    stdout().flush()?;
    println!();
    println!();

    println!("MIT License: ");
    println!("Copyright (c) 2025 Glowwy-Dev

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the \"Software\"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.");
    stdout().flush()?;

    println!();

    if errored {
        println!("Update process Failed Somewhere.");
        println!("You may need to delete some files your self.");
    } else {
        println!("Update Successful.");
        println!("We hope you have a great time continuing to use Vice!");
    }

    println!();

    presskeytoquit()
}

fn uninstall(old_path: String) -> Result<(), Box<dyn std::error::Error>> {
    println!("Final chance to exit.");
    stdout().flush()?;
    
    let mut seconds = 3;
    let mut elapsed_ms = 0;
    let tick_ms = 100;

    while event::poll(Duration::from_millis(0))? {
        let _ = event::read()?;
    }
    loop {
        if event::poll(Duration::from_millis(0))? {
            if let Event::Key(_) = event::read()? {
                return Ok(());
            }
        }

        if elapsed_ms % 1000 == 0 {
            print!("\rYou have {} seconds to press any key to exit.", seconds);
            stdout().flush()?;
            seconds -= 1;

            if seconds < 0 {
                println!();
                println!();
                break;
            }
        }

        thread::sleep(Duration::from_millis(tick_ms));
        elapsed_ms += tick_ms;
    }

    println!("Please do not unplug your device during the uninstall process.");
    println!();
    println!();

    //Close Vice if running
    stdout().flush()?;
    print!("Closing Vice...");

    let _ = close_vice(old_path.clone());

    print!("\rClosed Vice...         ");
    stdout().flush()?;
    println!();

    let mut errored = false;

    //save data deletion
    stdout().flush()?;
    println!();

    print!("Scheduling Save Data Deletion...");

    let path_save = std::env::var("APPDATA").unwrap_or_default() + "\\Vice";

    schedule_delete(&path_save.clone());

    print!("\rScheduled Deletion On Reboot...       ");
    println!();

    //Vice deletion
    stdout().flush()?;
    println!();

    print!("Deleting Application...");

    let res_app = delete_file(old_path.clone());

    match res_app {
        Ok(_) => {
            print!("\rDeleted Application...    ");
        }
        Err(e) => {
            if e.to_string().contains("Directory does not exist") {
                print!("\rApplication doesn't exist. This should not happen. If you can reliably reproduce this issue, please report it at https://github.com/Glowwy-Dev/Vice/issues");
                errored = true;
            } else {
                print!("\rFailed to delete app: {}", e);
                errored = true;
            }
        }
    }

    println!();
    println!();
    stdout().flush()?;

    if errored {
        println!("Deletion Failed Somewhere.");
        println!("You may need to delete some files your self.");
    } else {
        println!("Deletion Successful.");
        println!("Thank you for using Vice!");
    }

    println!();

    presskeytoquit()
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    terminal::enable_raw_mode()?;

    let args: Vec<String> = std::env::args().collect();

    if args.len() > 3 {
        let command = args[1].clone();
        let path = args[2].clone();
        let debug = args[3].clone();

        match command.as_str() {
            "update" => update(path, debug)?,
            "uninstall" => uninstall(path)?,
            _ => println!("Unknown command {}", command),
        }
    } else {
        println!("Meant to be ran like this:");
        println!("uninstall|update <path to Vice> <debug>");
    }

    terminal::disable_raw_mode()?;
    Ok(())
}