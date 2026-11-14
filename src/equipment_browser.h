#pragma once

// Browser I/O only. Project validation and all artwork live in C++; no virtual
// filesystem, network service, localStorage size assumption, or JS game model.
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
EM_JS(void,installEquipmentBridge,(int game),{
    const isGame=!!game, maxBytes=52428800;
    let epoch=0;
    function database() {
        return new Promise(function(resolve,reject) {
            const request=indexedDB.open('ashfall-equipment',1);
            request.onupgradeneeded=function(){request.result.createObjectStore('art');};
            request.onerror=function(){reject(request.error);};
            request.onblocked=function(){reject(new Error('Storage blocked'));};
            request.onsuccess=function(){resolve(request.result);};
        });
    }
    async function storage(mode,bytes) {
        const db=await database();
        try {
            return await new Promise(function(resolve,reject) {
                const tx=db.transaction('art',mode), store=tx.objectStore('art');
                const request=mode==='readonly'?store.get('applied'):(bytes?store.put(bytes,'applied'):store.delete('applied'));
                tx.oncomplete=function(){resolve(request.result);};
                tx.onerror=tx.onabort=function(){reject(tx.error);};
            });
        } finally { db.close(); }
    }
    let channel;
    try { channel=new BroadcastChannel('ashfall-equipment'); } catch(error) {}
    function changed(){if(channel) channel.postMessage('changed');}
    function send(bytes) {
        if (!bytes || !bytes.length || bytes.length>maxBytes) { Module._ashfall_art_status(4); return false; }
        const pointer=Module._ashfall_art_import_buffer(bytes.length);
        if(!pointer){Module._ashfall_art_status(4);return false;}
        HEAPU8.set(bytes,pointer);
        return !!Module._ashfall_art_import_file(bytes.length);
    }
    async function load() {
        const request=++epoch;
        try {
            const bytes=await storage('readonly');
            if (request!==epoch) return;
            if (bytes) send(new Uint8Array(bytes));
            else if (isGame) Module._ashfall_art_restore();
            else Module._ashfall_art_status(3);
        } catch(error) { if(request===epoch) Module._ashfall_art_status(2); }
    }
    let writes=Promise.resolve();
    function write(bytes) {
        writes=writes.catch(function(){}).then(function(){return storage('readwrite',bytes);});
        return writes;
    }
    Module.artStore={
        apply: async function(bytes) { ++epoch; await write(bytes); changed(); },
        clear: async function() { ++epoch; await write(null); changed(); },
        load: load
    };
    async function readFile(file) {
        if (!file) return;
        if(isGame && file.name.toLowerCase().endsWith('.ashrun') && Module.importRunFile) { await Module.importRunFile(file); return; }
        const request=++epoch;
        try {
            if(!file.size || file.size>maxBytes) {Module._ashfall_art_status(4);return;}
            const bytes=new Uint8Array(await file.arrayBuffer());
            if (request!==epoch) return;
            if (send(bytes) && isGame) {
                try { await Module.artStore.apply(bytes); Module._ashfall_art_status(1); }
                catch(error) { Module._ashfall_art_status(2); }
            }
        } catch(error) { if(request===epoch) Module._ashfall_art_status(4); }
        finally { Module.canvas.focus({preventScroll:true}); }
    }
    const input=document.getElementById(isGame?'equipment-file':'png-file');
    input.addEventListener('change',function(){const file=input.files[0];input.value="";readFile(file);});
    Module.canvas.addEventListener('dragover',function(event){event.preventDefault();});
    Module.canvas.addEventListener('drop',function(event){event.preventDefault();readFile(event.dataTransfer.files[0]);});
    if(isGame) {
        if(channel) channel.onmessage=load;
        load();
    }
});
EM_JS(void,applyEquipmentInBrowser,(const unsigned char* data,int size),{
    const bytes=HEAPU8.slice(data,data+size);
    Module.artStore.apply(bytes).then(function(){Module._ashfall_art_status(1);},function(){Module._ashfall_art_status(2);});
});
EM_JS(void,clearEquipmentInBrowser,(),{
    Module.artStore.clear().catch(function(){Module._ashfall_art_status(5);});
});
#endif
