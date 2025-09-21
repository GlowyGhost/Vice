pub(crate) fn devices() -> Vec<String> {
    vec!["1".to_string(), "2".to_string(), "3".to_string()]
}

pub(crate) fn start() {
    loop {
        println!("Doing audio...");
        std::thread::sleep(std::time::Duration::from_secs(10));
    }
}