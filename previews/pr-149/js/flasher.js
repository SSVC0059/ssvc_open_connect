const CHIP_FAMILY = "ESP32-S3";
const FIRMWARE_OFFSET = 0; 
const LOCAL_FIRMWARE_OFFSET = 0x10000; 
const FIRMWARE_BASE_PATH = "/ssvc_open_connect/firmware/";

const MANIFEST_TEMPLATE = {
    "name": "SSVC Open Connect",
    "home_assistant_domain": "ssvc_open_connect",
    "new_install_prompt_erase": true,
    "builds": [
        {
            "chipFamily": CHIP_FAMILY,
            "parts": [
                { "path": "", "offset": FIRMWARE_OFFSET }
            ]
        }
    ]
};

const compareVersions = (v1, v2) => {
    const parts1 = v1.split('.').map(Number);
    const parts2 = v2.split('.').map(Number);
    for (let i = 0; i < Math.max(parts1.length, parts2.length); i++) {
        const p1 = parts1[i] || 0;
        const p2 = parts2[i] || 0;
        if (p1 > p2) return 1;
        if (p1 < p2) return -1;
    }
    return 0;
};

function createManifestBlobUrl(version, filename) {
    const manifest = JSON.parse(JSON.stringify(MANIFEST_TEMPLATE));
    manifest.version = version;
    
    const firmwareFullUrl = FIRMWARE_BASE_PATH + filename; 
    manifest.builds[0].parts[0].path = firmwareFullUrl; 

    const json = JSON.stringify(manifest, null, 2); 
    const blob = new Blob([json], { type: "application/json" });
    const manifestBlobUrl = URL.createObjectURL(blob);

    return manifestBlobUrl;
}

function createInstallButton(buttonText, manifestUrl, container, isPrerelease = false) {
    const buttonWrapper = document.createElement('div');
    buttonWrapper.className = 'install-button-wrapper'; 
    if (isPrerelease) { buttonWrapper.id = 'test-button'; }