const resizer = document.getElementById('resizer');
const leftColumn = resizer.previousElementSibling;
const rightColumn = resizer.nextElementSibling;
const container = resizer.parentNode;

// Minimum and maximum width for left column in px
const minLeftWidth = 100;
const maxLeftWidth = container.clientWidth - 100;

let isResizing = false;

resizer.addEventListener('mousedown', e => {
  e.preventDefault();
  isResizing = true;
  document.body.style.userSelect = 'none'; // prevent text selection
});

document.addEventListener('mousemove', e => {
  if (!isResizing) return;

  // Calculate new width of left column relative to container
  const containerRect = container.getBoundingClientRect();
  let newLeftWidth = e.clientX - containerRect.left;

  // Clamp the width
  newLeftWidth = Math.max(minLeftWidth, Math.min(newLeftWidth, containerRect.width - minLeftWidth));

  // Set the left column's flex-basis (fixed width)
  leftColumn.style.flex = '0 0 ' + newLeftWidth + 'px';
  rightColumn.style.flex = '1 1 0'; // fill remaining space
});

document.addEventListener('mouseup', e => {
  if (isResizing) {
    isResizing = false;
    document.body.style.userSelect = ''; // restore user selection
  }
});
