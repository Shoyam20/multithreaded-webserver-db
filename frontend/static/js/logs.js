// Live refresh for the admin "Server logs" page.
(function () {
  var box = document.getElementById('logfeed');
  var toggle = document.getElementById('autorefresh');
  if (!box || !toggle) return;

  var timer = null;

  function term() { return document.getElementById('term'); }
  function scrollToBottom() { var t = term(); if (t) t.scrollTop = t.scrollHeight; }

  function refresh() {
    var t = term();
    // only follow new lines if the reader is already at the bottom
    var atBottom = !t || (t.scrollHeight - t.scrollTop - t.clientHeight < 40);
    var oldTop = t ? t.scrollTop : 0;

    fetch(box.dataset.feed, { credentials: 'same-origin' })
      .then(function (r) { return r.text(); })
      .then(function (html) {
        box.innerHTML = html;
        if (atBottom) scrollToBottom(); else if (term()) term().scrollTop = oldTop;
      })
      .catch(function () { /* server busy - try again next tick */ });
  }

  function apply() {
    if (timer) clearInterval(timer);
    timer = toggle.checked ? setInterval(refresh, 2000) : null;
  }

  toggle.addEventListener('change', apply);
  apply();
  scrollToBottom();
})();
