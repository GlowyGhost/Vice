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
    process::Command,
    net::TcpStream,
};
use reqwest::blocking::Client;
use serde::Deserialize;
use zip::ZipArchive;

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
    if !Path::new(&dir).exists() {
        return Err("Directory does not exist.".into());
    }

    return Ok(fs::remove_dir_all(&dir)?);
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
    #[cfg(target_os = "windows")]
    {
        path.extension()
            .map_or(false, |ext| ext.eq_ignore_ascii_case("exe"))
    }

    #[cfg(not(target_os = "windows"))]
    {
        use std::os::unix::fs::PermissionsExt;
        match fs::metadata(path) {
            Ok(metadata) => {
                let permissions = metadata.permissions();
                permissions.mode() & 0o111 != 0
            }
            Err(_) => false,
        }
    }
}
fn close_vice(path: String) -> Result<(), Box<dyn std::error::Error>> {
    let mut res = false;

    #[cfg(target_os = "windows")]
    {
        let process_name = Path::new(&path)
            .file_name()
            .and_then(|n| n.to_str())
            .unwrap_or(&path);

        let output = Command::new("tasklist")
            .arg("/FI")
            .arg(format!("IMAGENAME eq {}", process_name))
            .output()?;

        let stdout = String::from_utf8_lossy(&output.stdout).to_string();
        res = stdout.contains(process_name);
    }

    #[cfg(target_os = "linux")]
    {
        let status = Command::new("pgrep")
            .arg("-f")
            .arg(&path)
            .status()?;

        res = status.success();
    }

    if res {
        #[cfg(target_os = "windows")]
        {
            let process_name = Path::new(&path)
                .file_name()
                .and_then(|n| n.to_str())
                .unwrap_or(&path);

            let _ = Command::new("taskkill")
                .args(["/F", "/T", "/IM", process_name])
                .output();
        }

        #[cfg(target_os = "linux")]
        {
            let _ = Command::new("pkill")
                .arg("-9")
                .arg("-f")
                .arg(&path)
                .output();
        }
    }

    Ok(())
}
fn is_folder_unlocked(path: String) -> bool {
    let orig = Path::new(&path);
    let tmp = orig.with_extension("tmp_check_lock");

    match fs::rename(orig, &tmp) {
        Ok(_) => {
            let _ = fs::rename(&tmp, orig);
            true
        }
        Err(_) => false,
    }
}
fn install_update(old_path: String, debug: String) -> Result<(), Box<dyn std::error::Error>> {
    print!("\rPreparing to download...");
    let client = Client::new();
    let release: Release = client
        .get("https://api.github.com/repos/GlowyGhost/Vice/releases/latest")
        .header("User-Agent", "Vice-Updater")
        .send()?
        .json()?;

    #[cfg(target_os = "windows")]
    let target_asset = release.assets.iter().find(|a| a.name.ends_with("windows.zip"));

    #[cfg(target_os = "linux")]
    let target_asset = release.assets.iter().find(|a| a.name.ends_with("linux.zip"));

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

    print!("\rExtracting...");

    let file = File::open(&tmp_zip)?;
    let mut archive = ZipArchive::new(file)?;

    let extract_dir = Path::new("update_tmp");
    if extract_dir.exists() {
        fs::remove_dir_all(extract_dir)?;
    }
    fs::create_dir_all(extract_dir)?;

    #[cfg(target_os = "windows")]
    let expected_name = if debug == "false" { "Vice.exe" } else { "Vice-debug.exe" };
    #[cfg(any(target_os = "linux"))]
    let expected_name = if debug == "false" { "Vice" } else { "Vice-debug" };

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
        print!("\rBinary '{}' not found in archive.", expected_name);
        fs::remove_file(tmp_zip)?;
        fs::remove_dir_all(extract_dir)?;
        return Err("None".into());
    }

    let new_binary = extract_dir.join(expected_name);
    fs::copy(&new_binary, &old_path)?;
    print!("\rUpdated successfully to {}!", old_path);

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

    //Wait for Unlock
    print!("\rWaiting for Unlock...");
    stdout().flush()?;

    #[cfg(target_os = "windows")]
    let path_save = std::env::var("APPDATA").unwrap_or_default() + "\\Vice";

    #[cfg(target_os = "linux")]
    let path_save = std::env::var("HOME").unwrap_or_default() + "/.local/share/Vice";

    loop {
        if is_folder_unlocked(path_save.clone()) {
            break;
        }
        if !Path::new(&path_save).exists() {
            break;
        }

        thread::sleep(Duration::from_millis(1000));
    }

    print!("\rClosed Vice...         ");
    stdout().flush()?;
    println!();
    println!();

    let mut errored = false;

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
    #[cfg(target_os = "windows")]
    {
        let escaped = old_path.replace("&", "^&");

        let _ = Command::new("cmd")
            .args(["/C", "start", "", &escaped])
            .spawn();
    }

    #[cfg(target_os = "linux")]
    {
        let _ = Command::new(&old_path)
            .spawn();
    }

    print!("\rOpened new version...      ");
    stdout().flush()?;
    println!();
    println!();

    println!("MIT License: ");
    println!("Copyright (c) 2025 GlowyGhost

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

    //Wait for Unlock
    print!("\rWaiting for Unlock...");
    stdout().flush()?;

    #[cfg(target_os = "windows")]
    let path_save = std::env::var("APPDATA").unwrap_or_default() + "\\Vice";

    #[cfg(target_os = "linux")]
    let path_save = std::env::var("HOME").unwrap_or_default() + "/.local/share/Vice";

    loop {
        if is_folder_unlocked(path_save.clone()) {
            break;
        }
        if !Path::new(&path_save).exists() {
            break;
        }

        thread::sleep(Duration::from_millis(1000));
    }

    print!("\rClosed Vice...         ");
    stdout().flush()?;
    println!();

    let mut errored = false;

    //save data deletion
    stdout().flush()?;
    println!();

    print!("Deleting Save Data...");

    let res_save = delete_folder(path_save.clone());

    match res_save {
        Ok(_) => {
            print!("\rDeleted Save Data...     ");
        }
        Err(e) => {
            if e.to_string().contains("Directory does not exist") {
                print!("\rDirectory \"{}\" does not exist. It's been deleted already.", path_save);
            } else {
                print!("\rFailed to delete folder \"{}\": {}", path_save, e);
                errored = true;
            }
        }
    }
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
                print!("\rApplication doesn't exist. This should not happen. If you can reliably reproduce this issue, please report it at https://github.com/GlowyGhost/Vice/issues");
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