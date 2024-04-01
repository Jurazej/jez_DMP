const url = `wss://${window.location.hostname}/wss`;
let websocket;
let savelogin = false;
let inputpass;
let last_default_config = 0;
let last_restart = 0;
let nolDebug;
let segDebug;
navmenu(1);
window.addEventListener("load", onLoad);

//---------------------------------------------------------
// Navázání spojení se serverem
//---------------------------------------------------------
function initWebSocket() {
    console.log("Trying to open a WebSocket connection...");
    websocket = new WebSocket(url);
    websocket.onopen = onOpen;
    websocket.onclose = onClose;
    websocket.onmessage = onMessage;
}

function onLoad(event) {
    initWebSocket();
}

function onOpen(event) {
    console.log("Connection opened");
    document.getElementById("status").style.display = "none"; 
    document.getElementById("password").style.pointerEvents = "auto";
    document.getElementById("loginButton").style.opacity = "1";
    document.getElementById("loginButton").style.cursor = "auto";
    autologin();
}

function onClose(event) {
    console.log("Connection closed");
    setTimeout(initWebSocket, 2000);
}

//---------------------------------------------------------
// Funkce pro zpoždění v kódu
//---------------------------------------------------------
function sleep(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
}

//---------------------------------------------------------
// Zpracování přijaté WebSocket zprávy
//---------------------------------------------------------
async function onMessage(event) {
    console.log(event.data);
    const rmsg = JSON.parse(event.data);
    if (rmsg.type == "login") {
        if (rmsg.login_status == true) {
            if (savelogin == true) {
                document.cookie = "storedpass=" + inputpass;
                document.getElementById("status").style.color = "#09c212";
                document.getElementById("status").innerHTML= "Přihlášení proběhlo úspěšně";
                document.getElementById("status").style.display = "block";
            }
            document.getElementById("cvSsid").innerHTML = rmsg.currentconfig.ssid;
            document.getElementById("cvWifipass").innerHTML = rmsg.currentconfig.wifi_pass;
            document.getElementById("cvMdns").innerHTML = rmsg.currentconfig.mdns;
            document.getElementById("cvMaxcon").innerHTML = rmsg.currentconfig.maxcon;
            nolDebug = rmsg.debugdata.nol;
            segDebug = rmsg.debugdata.seg;
            await sleep(1000);
            document.getElementById("loginForm").style.display = "none";

        }
        else {
            document.getElementById("status").style.color = "#e01616";
            document.getElementById("status").innerHTML= "Přihlášení se nezdařilo";
            document.getElementById("status").style.display = "block";
            document.getElementById("loginButton").disabled = false;
        }
    }
    else if (rmsg.type == "config_change_status") {
        if (rmsg.login_status == true) {
            if (rmsg.change_success == 1) {
                alert("Změna nastavení proběhla ÚSPĚŠNĚ, změny se projeví AŽ PO RESTARTU zařízení");
            }
            else {
                alert("Změna nastavení se NEZDAŘILA");
            }   
        }
    }
    else if (rmsg.type == "default_config_status") {
        if (rmsg.login_status == true) {
            if (rmsg.success == 1) {
                alert("Obnovení základního nastavení proběhlo ÚSPĚŠNĚ, změny se projeví AŽ PO RESTARTU zařízení");
            }
            else {
                alert("Obnovení základního nastavení se NEZDAŘILO");
            }   
        }
    }
    else if (rmsg.type == "param_change_status") {
        if (rmsg.login_status == true) {  
            if (rmsg.change_success == 1) {
                alert("Parametrizace proběhla ÚSPĚŠNĚ");
            }
            else {
                alert("Parametrizace se NEZDAŘILA");
            }   
        }
    }             
}

//---------------------------------------------------------
// Odeslání WebSocket zprávy na server
//---------------------------------------------------------
function wSend(type,value=1) {
    websocket.send(JSON.stringify({
        "type": type,
        "value": value
    }));
}

//---------------------------------------------------------
// Automatické přihlášení pomocí hesla uloženého v cookies
//---------------------------------------------------------
function autologin(){
    let loadedpass = getCookie("storedpass");
    if (loadedpass != "") {
        inputpass = loadedpass;
        wSend("login",loadedpass);
        document.getElementById("status").innerText = "Pokus o automatické přihlášení"; 
        document.getElementById("status").style.display = "block"; 
    }
}

//---------------------------------------------------------
// Tlačítko pro přihlášení
//---------------------------------------------------------
function login() {
    savelogin = true;
    document.getElementById("loginButton").disabled = true;
    inputpass = document.getElementById("password").value
    if (inputpass != "") {
        wSend("login",inputpass);
        document.getElementById("status").style.display = "none";
    }
}

//---------------------------------------------------------
// Tlačítko pro restartování systému
//---------------------------------------------------------
async function restart(input) {
    if (last_restart== 0) {
        last_restart = 1;
        input.innerHTML = "Znovu pro potvrzení restartu";
        await sleep(3000);
        last_restart = 0;
        input.innerHTML = "Restartovat zařízení";

    }
    else {
        last_restart = 0;
        let jsnout = {};
        jsnout["reqpass"] = inputpass;
        wSend("restart",jsnout);
        alert("Zařízení bude restartováno"); 
        input.innerHTML = "Restartovat zařízení";     
    }
}

//---------------------------------------------------------
// Tlačítko pro nastavení výchozích hodnot nastavení
//---------------------------------------------------------
async function defaultConfig(input) {
    if (last_default_config == 0) {
        last_default_config = 1;
        input.innerHTML = "Znovu pro potvrzení obnovení";
        await sleep(3000);
        last_default_config = 0;
        input.innerHTML = "Obnovit základní nastavení";
    }
    else {
        last_default_config = 0;
        let jsnout = {};
        jsnout["reqpass"] = inputpass;
        wSend("default_config",jsnout);
        input.innerHTML = "Obnovit základní nastavení";
    }
}

//---------------------------------------------------------
// Získání hodnoty uložené v cookies
//---------------------------------------------------------
function getCookie(cname) {
    let name = cname + "=";
    let decodedCookie = decodeURIComponent(document.cookie);
    let ca = decodedCookie.split(";");
    for(let i = 0; i <ca.length; i++) {
        let c = ca[i];
        while (c.charAt(0) == " ") {
        c = c.substring(1);
        }
        if (c.indexOf(name) == 0) {
        return c.substring(name.length, c.length);
        }
    }
    return "";
}

//---------------------------------------------------------
// Získání hodnot ze vstupních polí formuláře nastavení a odeslání na server
//---------------------------------------------------------
function changeConfig() {
    let configSsid = document.getElementById("configSsid").value
    let configWifipass = document.getElementById("configWifipass").value
    let configMaxcon = document.getElementById("configMaxcon").value
    let configMdns = document.getElementById("configMdns").value
    let configAdminpass = document.getElementById("configAdminpass").value
    let cfgts = {};
    if (configSsid != "" || configWifipass != "" || configMaxcon != "" || configMdns != "" || configAdminpass != "") {
        if (configSsid != "") {
            cfgts["ssid"] = configSsid;
        }
        if (configWifipass != "") {
            cfgts["wifi_pass"] = configWifipass;
        }
        if (configMaxcon != "") {
            cfgts["maxcon"] = Number(configMaxcon);
        }
        if (configMdns != "") {
            cfgts["mdns"] = configMdns;
        }
        if (configAdminpass != "") {
            cfgts["admin_pass"] = configAdminpass;
        }
        cfgts["reqpass"] = inputpass;
        wSend("config",cfgts);
    }
}

//---------------------------------------------------------
// Kontrola délky vstupu
//---------------------------------------------------------
function checkLength(inp) {
    if (inp.value != "") {
        if ((inp.value.length > inp.maxLength) || (inp.value.length < inp.minLength)) {
            inp.value = "";
            alert(`Délka musí být od ${inp.minLength} do ${inp.maxLength} znaků`);
        }
    }
}

//---------------------------------------------------------
// Kontrola hodnoty vstupu
//---------------------------------------------------------
function checkValue(inp) {
    if (inp.value != "") {
        if ((Number(inp.value) > Number(inp.max)) || (Number(inp.value) < Number(inp.min))) {
            inp.value = "";
            alert(`Hondota musí být od ${inp.min} do ${inp.max}`);
        }
    }
}

//---------------------------------------------------------
// Kontrola hodnoty vstupu typu maska
//---------------------------------------------------------
function validateMaskInput(input) {
    const inputValue = input.value;
    const pattern = /^[01]+$/;
    const maxLength = input.getAttribute("wanted_length");
    if (!pattern.test(inputValue) || inputValue.length > maxLength) {
    input.value = inputValue.replace(/[^01]/g, '').slice(0, maxLength);
    }
}

//---------------------------------------------------------
// Generace vstupů podle počtu světlometů 
//---------------------------------------------------------
function generateLights(input) {
    if (Number(input.value) != 1) {
        const lightCount = input.value;
        const lightContainer = document.getElementById("lightContainer");
        lightContainer.innerHTML = "";
        for (let i = 1; i <= lightCount-1; i++) {
            const lightTable = document.createElement("table");
            const index = i+1;
            lightTable.innerHTML = `
            <tr>
            <td colspan="2"><h2>${index}. světlomet:</h2></td>
            </tr>
            <tr>
                <td>Počet driverů</td>
                <td><input type="number" id="parNod${index}" onblur="checkValue(this)" onchange="generateDrivers(this, ${index})" min="1" max="16"></td>
            </tr>
            <tr> 
                <td id="driverContainer${index}" class="container" colspan="2"></td>
            </tr>
            <tr>
                <td colspan="2"><h3>Rozdělení ${index}. světlometu na části:</h3></td>
            </tr>
            <tr>
                <td>Počet částí:</td>
                <td><input type="number" id="parNop${index}" onblur="checkValue(this)" onchange="generateParts(this, ${index})" min="1" max="5"></td>
            </tr>
            <tr> 
                <td id="partContainer${index}" class="container" colspan="2"></td>
            </tr>
            `;
            lightContainer.appendChild(lightTable);
        }
    }
    else {
        lightContainer.innerHTML = "";
    }
}

//---------------------------------------------------------
// Generace vstupů podle počtu driverů na světlometu
//---------------------------------------------------------
function generateDrivers(input, number) {
    if (Number(input.value) <= Number(input.max)) {
        const driverCount = input.value;
        const driverContainer = document.getElementById(`driverContainer${number}`);
        driverContainer.innerHTML = "";
        for (let i = 1; i <= driverCount; i++) {
            const driverTable = document.createElement("table");
            driverTable.innerHTML = `
            <tr>
                <td>Invertování ${i}. driveru</td>
                <td><input type="checkbox" id="parInv${number}_${i}"></td>
            </tr>
            <tr>
                <td>Adresa ${i}. driveru</td>
                <td><input type="number" id="parAdrod${number}_${i}" onblur="checkValue(this)" min="1" max="31"></td>
            </tr>
            <tr>
                <td>Maska ${i}. driveru</td>
                <td><input type="text" id="parMaskod${number}_${i}" onblur="checkLength(this)" oninput="validateMaskInput(this)" minlength="16" maxlength="16" wanted_length="16"></td>
            </tr>
            `;
            driverContainer.appendChild(driverTable);
        }
    }
}

//---------------------------------------------------------
// Generace vstupů podle počtu částí světlometu
//---------------------------------------------------------
function generateParts(input, number) {
    if (Number(input.value) <= Number(input.max)) {
        const partCount = input.value;
        const partContainer = document.getElementById(`partContainer${number}`);
        partContainer.innerHTML = "";

        for (let i = 1; i <= partCount; i++) {
            const partTable = document.createElement('table');
            partTable.innerHTML = `
            <tr>
                <td>Název ${i}. části světlometu</td>
                <td><input type="text" onblur="checkLength(this)" id="parNaop${number}_${i}" minlength="1" maxlength="30"></td>
            </tr>
            <tr>
                <td>Počátek ${i}. části světlometu</td>
                <td><input type="number" onblur="checkValue(this)" id="parSop${number}_${i}" min="0" max="128"></td>
            </tr>
            <tr>
                <td>Délka ${i}. části světlometu</td>
                <td><input type="number" onblur="checkValue(this)" id="parLop${number}_${i}" min="1" max="128"></td>
            </tr>
            `;
            partContainer.appendChild(partTable);
        }
    } 
}

//---------------------------------------------------------
// Generace prvků pro debug světlometu 
//---------------------------------------------------------
function generateDebug(input) {
    let data = {};
    if (inputpass == null) {
        data.reqpass = "empty";
    } else {
        data.reqpass = inputpass;
    }
    data.state = input.checked;
    wSend("debug_toggle",data);
    const lightContainer = document.getElementById("debugLightContainer");
    if (input.checked == true) {
        for (let i = 1; i <= nolDebug; i++) {
            const lightTable = document.createElement("table");
            lightTable.innerHTML = `
            <tr>
                <td colspan="2"><h2>${i}. světlomet:</h2></td>
            </tr>
            `;
            for (let j = 1; j <= segDebug[i]; j++) {
                lightTable.innerHTML = lightTable.innerHTML +`
                <tr>
                    <td>${j}. segment</td>
                    <td><input type="checkbox" id="debug${i}_${j}" onclick="debugSend(this, ${i}, ${j})"></td>
                </tr>
                `;
            }
            lightContainer.appendChild(lightTable);
        }
    }
    else {
        lightContainer.innerHTML =""
    }
}

//---------------------------------------------------------
// Odeslat informaci o rozsvícení / zhasnutí segmentu
// při debugu parametrizace světlometu
//---------------------------------------------------------
function debugSend(input, light, index) {
    let formData = {};
    if (inputpass == null) {
        formData.reqpass = "empty";
    } else {
        formData.reqpass = inputpass;
    }
    formData.index = index;
    formData.light = light;
    formData.state = input.checked;
    wSend("debug",formData);
}

//---------------------------------------------------------
// Získání veškerých parametrů pro parametrizaci světlometu
//---------------------------------------------------------
function collectParamFormData() {
    const paramFormData = {};
    if (inputpass == null) {
        paramFormData.reqpass = "empty";
    } else {
        paramFormData.reqpass = inputpass;
    }

    let numLights = Number(document.querySelector(".select").value);
    paramFormData.numLights = numLights;

    const lights = [];
    for (let i = 1; i <= numLights; i++) {
        const light = {};

        let numDrivers = document.getElementById(`parNod${i}`).value;
        if (numDrivers === "") {
            alert(`Chyba: Počet driverů ${i}. světla je prázdný`);
            return;
        }
        else {
            numDrivers = Number(numDrivers);
        }
        light.numDrivers = numDrivers;

        const drivers = [];
        for (let j = 1; j <= numDrivers; j++) {
            const driver = {};
            driver.inverted = document.getElementById(`parInv${i}_${j}`).checked;
            driver.address = document.getElementById(`parAdrod${i}_${j}`).value;
            if (driver.address === "") {
                alert(`Chyba: Adresa ${j}. driveru u ${i}. světla je prázdný`);
                return;
            }
            else {
                driver.address = Number(driver.address);
            }
            driver.mask = document.getElementById(`parMaskod${i}_${j}`).value;
            if (driver.mask === "") {
                alert(`Chyba: Maska ${j}. driveru u ${i}. světla je prázdná`);
                return;
            }
            drivers.push(driver);
        }
        light.drivers = drivers;

        let numParts = document.getElementById(`parNop${i}`).value;
        if (numParts === "") {
            alert(`Chyba: Počet částí u ${i}. světla je prázdný`);
            return;
        }
        else {
            numParts = Number(numParts);
        }
        light.numParts = numParts;

        const parts = [];
        for (let k = 1; k <= numParts; k++) {
            const part = {};
            part.start = document.getElementById(`parSop${i}_${k}`).value;
            if (part.start === "") {
                alert(`Chyba: Počátek ${k}. části u ${i}. světlometu je prázdný`);
                return;
            }
            else {
                part.start = Number(part.start);
            }
            part.length = document.getElementById(`parLop${i}_${k}`).value;
            if (part.length === "") {
                alert(`Chyba: Délka ${k}. části u ${i}. světlometu je prázdná`);
                return;
            }
            else {
                part.length = Number(part.length);
            }
            part.name = document.getElementById(`parNaop${i}_${k}`).value;
            if (part.name === "") {
                alert(`Chyba: Název ${k}. části u ${i}. světlometu je prázdný`);
                return;
            }
            parts.push(part);
        }
        light.parts = parts;
        lights.push(light);
    }
    paramFormData.lights = lights;
    console.log(JSON.stringify(paramFormData));
    wSend("light_params",paramFormData);
}

//---------------------------------------------------------
// Ovládání navigačního menu
//---------------------------------------------------------
function navmenu(id) {
    if (id == 1)    {
        document.getElementById("nav1").style.backgroundColor = "#100e91";
        document.getElementById("nav2").style.backgroundColor = "#2a3fc9";
        document.getElementById("nav3").style.backgroundColor = "#2a3fc9";
        document.getElementById("settingsForm").style.display = "block";
        document.getElementById("paramForm").style.display = "none";
        document.getElementById("debugForm").style.display = "none";
    }
    else if (id == 2) {
        document.getElementById("nav2").style.backgroundColor = "#100e91";
        document.getElementById("nav1").style.backgroundColor = "#2a3fc9";
        document.getElementById("nav3").style.backgroundColor = "#2a3fc9";
        document.getElementById("settingsForm").style.display = "none";
        document.getElementById("paramForm").style.display = "block";
        document.getElementById("debugForm").style.display = "none";
    }
    else if (id == 3) {
        document.getElementById("nav3").style.backgroundColor = "#100e91";
        document.getElementById("nav1").style.backgroundColor = "#2a3fc9";
        document.getElementById("nav2").style.backgroundColor = "#2a3fc9";
        document.getElementById("settingsForm").style.display = "none";
        document.getElementById("paramForm").style.display = "none";
        document.getElementById("debugForm").style.display = "block";
    }
}