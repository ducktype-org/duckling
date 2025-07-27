// Save and restore scroll positions
window.addEventListener('beforeunload', function() {
    localStorage.setItem('mainPanelScroll', document.getElementById('main-panel').scrollTop);
    localStorage.setItem('sidePanelScroll', document.getElementById('side-panel').scrollTop);
});

window.addEventListener('load', function() {
    const mainPanel = document.getElementById('main-panel');
    const sidePanel = document.getElementById('side-panel');
    
    const savedMainScroll = localStorage.getItem('mainPanelScroll');
    const savedSideScroll = localStorage.getItem('sidePanelScroll');
    
    if (savedMainScroll !== null) {
        mainPanel.scrollTop = parseInt(savedMainScroll);
    }
    if (savedSideScroll !== null) {
        sidePanel.scrollTop = parseInt(savedSideScroll);
    }
});

function handleComponentClick(event, componentId) {
    let clickType = 'CLICK';
    
    if (event.ctrlKey && event.shiftKey) {
        clickType = 'CLICK_INTERACTIVE_ROLLBACK';
    } else if (event.ctrlKey) {
        clickType = 'CLICK_INTERACTIVE';
    }

    fetch('/click', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify({
            component_id: componentId,
            click_type: clickType
        })
    })
    .then(response => response.json())
    .then(data => {
        window.location.reload();
    })
    .catch(error => console.error('Error:', error));
}

function closeSideInfo(sideInfoId) {
    fetch('/close_side_info', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify({
            side_info_id: sideInfoId
        })
    })
    .then(response => response.json())
    .then(data => {
        window.location.reload();
    })
    .catch(error => console.error('Error:', error));
}

function getEdge(sideInfoId, edgeId) {
    fetch('/get_edge', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify({
            side_info_id: sideInfoId,
            edge_id: edgeId
        })
    })
    .then(response => response.json())
    .then(data => {
        window.location.reload();
    })
    .catch(error => console.error('Error:', error));
}

function highlightTag(tag) {
    document.querySelectorAll('.code-component').forEach(component => {
        const tagsStr = component.dataset.tags;
        if (tagsStr) {
            const tags = tagsStr.split(',').filter(t => t).map(t => parseInt(t));
            if (tags.includes(tag)) {
                component.classList.add('highlighted');
            }
        }
    });
}

function unhighlightTag(tag) {
    document.querySelectorAll('.code-component.highlighted').forEach(component => {
        component.classList.remove('highlighted');
    });
}
