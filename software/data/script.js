let processingComplete = false;

window.onload = function () {
    fetchFileList();
    fetchStorageInfo();
    setInterval(fetchStorageInfo, 5000);
    document.getElementById('fileInput').addEventListener('change', function (e) {
        const files = e.target.files;
        const uploadContainer = document.getElementById('uploadProgressContainer');
        uploadContainer.innerHTML = ''; // alte Fortschrittsanzeigen löschen
      
        let completedUploads = 0;
        const totalUploads = files.length;
      
        for (const file of files) {
          const progressWrapper = document.createElement('div');
          progressWrapper.style.marginBottom = '10px';
      
          const label = document.createElement('div');
          label.textContent = `⬆️ ${file.name}`;
          label.style.fontSize = '14px';
          label.style.marginBottom = '4px';
      
          const progress = document.createElement('progress');
          progress.max = 100;
          progress.value = 0;
          progress.style.width = '100%';
      
          progressWrapper.appendChild(label);
          progressWrapper.appendChild(progress);
          uploadContainer.appendChild(progressWrapper);
      
          const xhr = new XMLHttpRequest();
          xhr.open('POST', '/upload', true);
      
          xhr.upload.onprogress = function (event) {
            if (event.lengthComputable) {
              const percent = (event.loaded / event.total) * 100;
              progress.value = percent;
            }
          };
      
          xhr.onload = function () {
            if (xhr.status === 200) {
              label.textContent += ' ✅ Erfolgreich';
            } else {
              label.textContent += ' ❌ Fehler';
            }
      
            completedUploads++;
            if (completedUploads === totalUploads) {
              // Wenn ALLE Uploads fertig sind, aktualisiere die Liste
              location.reload();
            }
          };
      
          const formData = new FormData();
          formData.append('file', file, file.name);
          xhr.send(formData);
        }
      });
      
      
  };
  
  let uploadedFiles = [];
  let isPlaying = false;
  const excludedFiles = ["HS-Wismar_Logo-FIW_V1_RGB.png","script.js", "HTML_Server.html","freq.cfg"];

    
  
  function switchTab(tabId) {
    document.querySelectorAll(".tab-content").forEach(tc => {
      tc.classList.remove("active");
      tc.style.display = "none";
    });
    document.querySelectorAll(".tab").forEach(tab => {
      tab.classList.remove("active");
    });
    document.getElementById(tabId).classList.add("active");
    document.getElementById(tabId).style.display = "block";
    const index = tabId === "mainTab" ? 0 : 1;
    document.querySelectorAll(".tab")[index].classList.add("active");
  }

  function loadFileList() {
    processingComplete = false;
    fetch("/getFiles")
      .then(res => res.json())
      .then(data => {
        const fileList = document.getElementById("fileList");
        fileList.innerHTML = "";
        data.files.forEach(file => {
          const li = document.createElement("li");
          li.textContent = file;
          fileList.appendChild(li);
        });
      });
  }
  
  function fetchFileList() {
    fetch('/getFiles')
      .then(response => response.json())
      .then(data => {
        uploadedFiles = data.files.map(file => ({ name: file, selected: false, channel: "CH_A" }));
        displayFiles();
      })
      .catch(error => console.error('Fehler beim Abrufen der Dateiliste: ', error));
  }
  
  function isExcluded(filename) {
    return excludedFiles.includes(filename);
  }

  function updateStorage() {
    fetch("/storage")
      .then(res => res.json())
      .then(data => {
        const percent = (data.used / data.total * 100).toFixed(1);
        document.getElementById("storageInfo").textContent = `Speicher: ${percent}% genutzt (${(data.used / 1024).toFixed(1)} KB von ${(data.total / 1024).toFixed(1)} KB)`;
      });
  }
  
  
  function displayFiles() {
    const fileListElement = document.getElementById("fileList");
    if (!fileListElement) return;
    fileListElement.innerHTML = "";
    uploadedFiles.filter(file => !isExcluded(file.name)).forEach((file) => {
      const li = document.createElement("li");
      li.setAttribute('draggable', true);
      const fileName = document.createElement("span");
      fileName.textContent = file.name;
      li.appendChild(fileName);
  
      const actionContainer = document.createElement("div");
      actionContainer.style.display = "flex";
      actionContainer.style.gap = "5px";
      actionContainer.style.alignItems = "center";
  
      const checkbox = document.createElement("input");
      checkbox.type = "checkbox";
      checkbox.checked = file.selected;
      checkbox.addEventListener('change', () => {
        const f = uploadedFiles.find(f => f.name === file.name);
        if (f) f.selected = checkbox.checked;
      });
      actionContainer.appendChild(checkbox);
  
      const channelSelect = document.createElement("select");
      channelSelect.className = "channel-select";
      ["CH_A", "CH_B", "CH_C", "CH_D"].forEach(ch => {
        const option = document.createElement("option");
        option.value = ch;
        option.textContent = ch;
        channelSelect.appendChild(option);
      });
      channelSelect.value = file.channel;
      channelSelect.addEventListener('change', () => {
        const f = uploadedFiles.find(f => f.name === file.name);
        if (f) f.channel = channelSelect.value;
      });
      actionContainer.appendChild(channelSelect);
  
      const deleteIcon = document.createElement("span");
      deleteIcon.textContent = "🗑️";
      deleteIcon.style.cursor = "pointer";
      deleteIcon.addEventListener('click', () => deleteFile(file.name));
      actionContainer.appendChild(deleteIcon);
  
      li.appendChild(actionContainer);
      fileListElement.appendChild(li);
    });
    addDragAndDrop(fileListElement);
  }
  
  function fetchStorageInfo() {
    fetch('/storage')
      .then(response => response.json())
      .then(data => {
        const storageElement = document.getElementById('storageInfo');
        if (!storageElement) return;
        const used = (data.used / 1024).toFixed(2);
        const total = (data.total / 1024).toFixed(2);
        const percent = ((data.used / data.total) * 100).toFixed(1);
        storageElement.textContent = `Speicher: ${used} KB / ${total} KB (${percent}%)`;
      })
      .catch(error => {
        const storageElement = document.getElementById('storageInfo');
        if (storageElement) storageElement.textContent = 'Speicher: Fehler';
        console.error('Fehler beim Abrufen des Speicherstands:', error);
      });
  }
  
  function processFiles() {
    // Vor der Verarbeitung: Kanaldaten auf dem Server zurücksetzen
    fetch('/resetChannels', { method: 'POST' })
      .then(() => {
        const selectedFiles = uploadedFiles.filter(file => file.selected);
        if (selectedFiles.length === 0) {
          alert("Keine Dateien ausgewählt. Bitte markieren Sie die gewünschten Dateien.");
          return;
        }
        document.getElementById('processingPopup2').style.display = 'block';
        let channels = selectedFiles.map(file => ({ name: file.name, channel: file.channel }));
        const formData = new FormData();
        formData.append('channels', JSON.stringify(channels));
        fetch('/processFiles', {
          method: 'POST',
          body: formData,
        })
        .then(response => {
          if (!response.ok) throw new Error('Verarbeitung fehlgeschlagen');
          return response.json();
        })
        .then(data => {
            displayResults(data.results);
            document.getElementById('processingPopup2').style.display = 'none';
            processingComplete = true;
          })
          .catch(error => {
            console.error('Fehler beim Senden der Dateien: ', error);
            document.getElementById('processingPopup2').style.display = 'none';
            alert('Verarbeitung fehlgeschlagen. Bitte erneut versuchen.');
          });
      });
  }
  
  function displayResults(results) {
    let resultArea = document.getElementById('resultArea');
    if (!resultArea) return;
    if (!results || results.length === 0) {
      resultArea.innerHTML = "<p>Keine Ergebnisse.</p>";
      return;
    }
    let table = "<table id='resultTable'><tr><th>Dateiname</th><th>Channel</th><th>Extrahierte Zahlen</th><th>Zahlanzahl</th><th>Fehler</th></tr>";
    results.forEach(result => {
      table += "<tr>";
      table += `<td>${result.filename}</td>`;
      table += `<td>${result.channel}</td>`;
      table += `<td>${result.numbers ? result.numbers.join(", ") : ""}</td>`;
      table += `<td>${result.numberCount ?? ""}</td>`;
      table += `<td>${result.error ?? ""}</td>`;
      table += "</tr>";
    });
    table += "</table>";
    resultArea.innerHTML = table;
  }
  
  function togglePlayPause() {
    if (!processingComplete) {
      alert("Bitte zuerst die Datenverarbeitung starten, bevor Sie abspielen.");
      return;
    }
  
    const popup = document.getElementById("processingPopup2");
  
    // Popup anzeigen und Text anpassen
    popup.style.display = "block";
    popup.querySelector("p").textContent = "Wiedergabe wird gestartet...";
  
    fetch('/play', { method: 'POST' })
      .then(response => {
        if (!response.ok) throw new Error('Fehler beim Starten der Wiedergabe');
        return response.text();
      })
      .then(data => {
        console.log('Wiedergabe gestartet:', data);
        popup.querySelector("p").textContent = "Wiedergabe läuft...";
      })
      .catch(error => {
        console.error(error);
        popup.querySelector("p").textContent = "Fehler beim Abspielen!";
      })
      .finally(() => {
        // Popup nach 3 Sekunden wieder ausblenden
        setTimeout(() => {
          popup.style.display = "none";
        }, 3000);
      });
  }
  
  
  function toggleInfoPopup() {
    const infoPopup = document.getElementById('infoPopup');
    if (infoPopup) infoPopup.style.display = infoPopup.style.display === 'none' ? 'block' : 'none';
  }
  
  function deleteFile(fileName) {
    processingComplete = false;
    if (!confirm(`Möchtest du die Datei "${fileName}" wirklich löschen?`)) return;
    fetch(`/delete?name=${encodeURIComponent(fileName)}`, { method: 'DELETE' })
      .then(response => {
        if (response.ok) fetchFileList();
        else alert("Fehler beim Löschen der Datei.");
      })
      .catch(error => console.error('Fehler beim Löschen der Datei: ', error));
  }
  
  function addDragAndDrop(list) {
    let draggedItem = null;
    list.addEventListener('dragstart', (e) => {
      draggedItem = e.target;
      draggedItem.classList.add('dragging');
    });
    list.addEventListener('dragover', (e) => {
      e.preventDefault();
      const afterElement = getDragAfterElement(list, e.clientY);
      if (afterElement == null) {
        list.appendChild(draggedItem);
      } else {
        list.insertBefore(draggedItem, afterElement);
      }
    });
    list.addEventListener('dragend', () => {
      draggedItem.classList.remove('dragging');
      updateFileOrder();
    });
  }
  
  function getDragAfterElement(container, y) {
    const draggableElements = [...container.querySelectorAll('li:not(.dragging)')];
    return draggableElements.reduce((closest, child) => {
      const box = child.getBoundingClientRect();
      const offset = y - box.top - box.height / 2;
      return offset < 0 && offset > closest.offset ? { offset, element: child } : closest;
    }, { offset: Number.NEGATIVE_INFINITY }).element;
  }
  
  function updateFileOrder() {
    const items = document.querySelectorAll('#fileList li');
    let newOrder = [];
    items.forEach(item => {
      const fileName = item.querySelector("span").textContent;
      const file = uploadedFiles.find(f => f.name === fileName);
      if (file) newOrder.push(file);
    });
    uploadedFiles = newOrder;
  }
  
  
  function handleFileUpload(event) {
    const files = event.target.files;
    if (!files || files.length === 0) return;
  
    const progressPopup = document.getElementById('progressPopup');
    const progressBar = document.getElementById('fileUploadProgress');
    const timeRemaining = document.getElementById('timeRemaining');
  
    progressPopup.style.display = 'block';
    progressBar.value = 0;
    timeRemaining.textContent = 'Verbleibende Zeit: --:--';
  
    let uploaded = 0;
    const startTime = Date.now();
  
    function uploadNext(index) {
      if (index >= files.length) {
        progressPopup.style.display = 'none';
        fetchFileList();
        return;
      }
  
      const file = files[index];
  
      fetch('/upload', {
        method: 'POST',
        body: file
      })
        .then(() => {
          uploaded++;
          const percent = Math.round((uploaded / files.length) * 100);
          progressBar.value = percent;
          const elapsed = (Date.now() - startTime) / 1000;
          const remaining = (elapsed / uploaded) * (files.length - uploaded);
          const minutes = Math.floor(remaining / 60);
          const seconds = Math.floor(remaining % 60);
          timeRemaining.textContent = `Verbleibende Zeit: ${minutes}:${seconds.toString().padStart(2, '0')}`;
          uploadNext(index + 1);
        })
        .catch(error => {
          console.error('Fehler beim Upload:', error);
          alert('Upload fehlgeschlagen.');
          progressPopup.style.display = 'none';
        });
    }
  
    uploadNext(0);
  }

  function togglePlayPause() {
    fetch('/play', { method: 'POST' })
      .then(response => {
        if (!response.ok) throw new Error('Fehler beim Starten der Wiedergabe');
        return response.text();
      })
      .then(data => console.log('Wiedergabe gestartet:', data))
      .catch(error => console.error(error));
  }

  function toggleFreqPopup() {
    const popup = document.getElementById('freqPopup');
    const msg = document.getElementById('freqPopupMsg');
    if (popup.style.display === 'none' || popup.style.display === '') {
      // Beim Öffnen aktuelle Frequenz laden
      fetch('/getFrequency')
        .then(res => res.json())
        .then(data => {
          document.getElementById('freqInput').value = data.frequency ?? 100;
          msg.textContent = '';
        });
      popup.style.display = 'block';
    } else {
      popup.style.display = 'none';
      msg.textContent = '';
    }
  }

  function saveFrequency() {
    const freq = parseInt(document.getElementById('freqInput').value, 10);
    const msg = document.getElementById('freqPopupMsg');
    if (isNaN(freq) || freq < 1 || freq > 1000) {
      msg.textContent = "Bitte eine gültige Frequenz (1-1000 Hz) eingeben.";
      msg.style.color = "red";
      return;
    }
    const payload = JSON.stringify({ frequency: freq });
    console.log("Sende an /setFrequency:", payload); // Debug-Ausgabe
    fetch('/setFrequency', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: payload
    })
      .then(res => {
        if (res.ok) return res.text();
        return res.text().then(text => { throw new Error(text); });
      })
      .then(() => {
        msg.textContent = "Frequenz gespeichert!";
        msg.style.color = "green";
        setTimeout(toggleFreqPopup, 1000);
      })
      .catch(err => {
        msg.textContent = err.message || "Fehler beim Speichern!";
        msg.style.color = "red";
      });
  }

