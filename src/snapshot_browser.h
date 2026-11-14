#pragma once

// Async I/O glue only: C++ owns/validates all game and document state. This
// database is intentionally independent of explicitly applied equipment art.
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
EM_JS(void,installSnapshotBridge,(int editor),{
    const isEditor=!!editor, key=isEditor?'editor':'run', maximum=isEditor?52428928:131072;
    let epoch=0, writes=Promise.resolve(), pending=null, draining=false, hasLock=false, acquiring=false;
    function database() {
        return new Promise(function(resolve,reject) {
            let abandoned=false;
            const request=indexedDB.open('ashfall-saves',1);
            request.onupgradeneeded=function(){
                if(!request.result.objectStoreNames.contains('snapshots')) request.result.createObjectStore('snapshots');
            };
            request.onerror=function(){abandoned=true;reject(request.error);};
            request.onblocked=function(){abandoned=true;reject(new Error('Storage blocked'));};
            request.onsuccess=function(){if(abandoned) request.result.close();else resolve(request.result);};
        });
    }
    async function storage(mode,bytes) {
        const db=await database();
        try {
            return await new Promise(function(resolve,reject) {
                const tx=db.transaction('snapshots',mode), store=tx.objectStore('snapshots');
                const request=mode==='readonly'?store.get(key):(bytes?store.put(bytes,key):store.delete(key));
                tx.oncomplete=function(){resolve(request.result);};
                tx.onerror=tx.onabort=function(){reject(tx.error);};
            });
        } finally { db.close(); }
    }
    function status(operation,value) { if(operation===epoch) Module._ashfall_snapshot_status(value); }
    function send(bytes,operation) {
        if(operation!==epoch) return;
        if(!(bytes instanceof Uint8Array) || !bytes.length || bytes.length>maximum) { status(operation,-3); return; }
        const pointer=Module._ashfall_snapshot_buffer(bytes.length);
        if(!pointer) { status(operation,-3); return; }
        HEAPU8.set(bytes,pointer); Module._ashfall_snapshot_receive(bytes.length);
    }
    async function read() {
        const operation=++epoch;
        // A quick Load after Save must observe that completed write, not the
        // previous record. Failed writes don't poison future operations.
        try {
            await writes.catch(function(){});
            const bytes=await storage('readonly');
            if(bytes===undefined) status(operation,0);else send(bytes,operation);
        } catch(error) { status(operation,-1); }
    }
    function load() {
        if(!isEditor || !navigator.locks || hasLock) { read(); return; }
        if(acquiring) return;
        acquiring=true;
        const operation=++epoch;
        // Do not let a second editor silently overwrite the first tab's draft.
        // Closing/navigating the owner releases this browser-managed lease.
        navigator.locks.request('ashfall-editor-autosave',{ifAvailable:true},function(lock){
            acquiring=false;
            if(!lock) {status(operation,-4);return;}
            hasLock=true; read();
            return new Promise(function(){});
        }).catch(function(){acquiring=false;status(operation,-1);});
    }
    function write(bytes) {
        const operation=++epoch;
        if(isEditor && navigator.locks && !hasLock) {status(operation,-4);return;}
        // Keep at most one active write and one latest pending snapshot. A
        // slow/quota-limited database must not accumulate whole-project copies.
        pending={bytes:bytes,operation:operation};
        if(draining) return;
        draining=true;
        writes=(async function(){
            let failure=null;
            try {
                while(pending) {
                    const job=pending; pending=null;
                    try {
                        await storage('readwrite',job.bytes); failure=null;
                        status(job.operation,job.bytes?1:2);
                    } catch(error) {failure=error;status(job.operation,-2);}
                }
            } finally {draining=false;}
            if(failure) throw failure;
        })();
        writes.catch(function(){});
    }
    async function importFile(file) {
        if(!file) return;
        const operation=++epoch;
        try {
            if(!file.size || file.size>maximum) { status(operation,-3); return; }
            send(new Uint8Array(await file.arrayBuffer()),operation);
        } catch(error) { status(operation,-1); }
        finally { Module.canvas.focus({preventScroll:true}); }
    }
    Module.snapshotStore={load:load,write:write,clear:function(){write(null);},cancel:function(){++epoch;},
        leave:function(){
            const navigate=function(){Module.allowEditorExit=true;window.location.href='index.html';};
            writes.then(navigate,navigate); // Explicit Back waits; browser/tab shutdown is best effort.
        }
    };
    if(isEditor) {
        document.addEventListener('visibilitychange',function(){if(document.hidden) Module._ashfall_snapshot_flush();});
        window.addEventListener('pagehide',function(){Module._ashfall_snapshot_flush();});
        load();
    } else {
        Module.importRunFile=importFile;
        const input=document.getElementById('run-file');
        input.addEventListener('change',function(){const file=input.files[0];input.value=String();importFile(file);});
    }
});
EM_JS(void,writeSnapshotInBrowser,(const unsigned char* data,int size),{
    // Copy before C++ releases/reuses its staging memory, never retain a heap view.
    Module.snapshotStore.write(HEAPU8.slice(data,data+size));
});
EM_JS(void,downloadSnapshot,(const unsigned char* data,int size),{
    const url=URL.createObjectURL(new Blob([HEAPU8.slice(data,data+size)],{type:'application/octet-stream'}));
    const link=document.createElement('a'); link.href=url; link.download='run.ashrun';
    document.body.appendChild(link);link.click();link.remove();
    setTimeout(function(){URL.revokeObjectURL(url);},30000);
});
#endif
