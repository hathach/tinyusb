(async () => {

  const uiResizer = document.getElementById('resizer');
  const uiLeftColumn = uiResizer.previousElementSibling;
  const uiRightColumn = uiResizer.nextElementSibling;
  const uiParent = uiResizer.parentElement;

  let isResizing = false;
  let abortSignal = null;

  function onMouseMove(e) {
    // we resize the columns by applying felx: <ratio> to the columns

    // compute the percentage the mouse is in the parent
    const percentage = (e.clientX - uiParent.offsetLeft) / uiParent.clientWidth;
    // clamp the percentage between 0.1 and 0.9
    const clampedPercentage = Math.max(0.1, Math.min(0.9, percentage));
    // set the flex property of the columns
    uiLeftColumn.style.flex = `${clampedPercentage}`;
    uiRightColumn.style.flex = `${1 - clampedPercentage}`;
  }

  function onMouseUp(e) {
    // restore user selection
    document.body.style.userSelect = '';

    // remove the mousemove and mouseup events
    if (abortSignal) {
      abortSignal.abort();
      abortSignal = null;
    }
  }

  uiResizer.addEventListener('mousedown', e => {
    e.preventDefault();
    isResizing = true;

    // prevent text selection
    document.body.style.userSelect = 'none';

    // register the mousemove and mouseup events
    abortSignal = new AbortController();
    document.addEventListener('mousemove', onMouseMove, { signal: abortSignal.signal });
    document.addEventListener('mouseup', onMouseUp, { signal: abortSignal.signal });
  });

})();
