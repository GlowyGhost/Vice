use std::{
    collections::HashMap,
    sync::{Mutex, atomic::{AtomicBool, Ordering}},
    thread,
    time::Duration,
};
use once_cell::sync::Lazy;
use serde::Serialize;

use crate::files;

#[derive(Default, Clone, Serialize, Debug)]
pub(crate) struct Data {
    pub(crate) system: HashMap<String, Vec<f32>>,
    pub(crate) app: HashMap<String, Vec<f32>>,
    pub(crate) general: HashMap<String, f32>,
}

#[link(name = "performance")]
extern "C" {
    fn get_sys_cpu() -> f32;
    fn get_app_cpu() -> f32;
    fn get_sys_ram() -> u64;
    fn get_app_ram() -> u64;
    fn get_total_ram() -> u64;
}

static RUN_MONITOR: AtomicBool = AtomicBool::new(true);
static PERFORMANCE: Lazy<Mutex<Data>> = Lazy::new(|| Mutex::new(Data::default()));


fn run_loop() {
    loop {
        if !RUN_MONITOR.load(Ordering::SeqCst) {
            break;
        }

        let total_ram: u64 = unsafe { get_total_ram() };
        let used_ram: u64 = unsafe { get_sys_ram() };
        let app_cpu: f32 = unsafe { get_app_cpu() };
        let avg_cpu: f32 = unsafe { get_sys_cpu() };
        let app_ram: u64 = unsafe { get_app_ram() };

        let mut per = PERFORMANCE.lock().unwrap();

        per.system.entry("cpu".into()).or_default().push((avg_cpu * 100.0).round() / 100.0);
        per.system.entry("ram".into()).or_default().push(used_ram as f32);
        per.app.entry("cpu".into()).or_default().push((app_cpu * 100.0).round() / 100.0);
        per.app.entry("ram".into()).or_default().push(app_ram as f32);

        per.general.insert("ram".into(), total_ram as f32);
        for vec in per.system.values_mut() {
            while vec.len() > 10 {
                vec.remove(0);
            }
        }

        for vec in per.app.values_mut() {
            while vec.len() > 10 {
                vec.remove(0);
            }
        }

        thread::sleep(Duration::from_secs(5));
    }
}

pub(crate) fn get_data() -> Data {
    PERFORMANCE.lock().unwrap().clone()
}

pub(crate) fn clear_data() {
    let mut per = PERFORMANCE.lock().unwrap();
    per.system.clear();
    per.app.clear();
    per.general.clear();
}

pub(crate) fn start() {
    if files::get_settings().monitor == true {
        if let Err(e) = thread::Builder::new()
            .name("performence-monitor".to_string())
            .spawn(|| run_loop()) {
            eprintln!("Failed to spawn performence monitor thread: {}", e);
        }
    }
}

pub(crate) fn change_bool(new_value: bool) {
    let old_val: bool = RUN_MONITOR.load(Ordering::SeqCst);

    RUN_MONITOR.store(new_value, Ordering::SeqCst);

    if old_val == false && new_value == true {
        start();
    }
}