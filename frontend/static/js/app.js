// Small helpers used by the templates. No framework needed.

// ---- login / signup pop-ups ----
function openModal(id)  { document.getElementById(id).classList.add('open'); }
function closeModal(id) { document.getElementById(id).classList.remove('open'); }

document.addEventListener('click', function (e) {
  if (e.target.classList && e.target.classList.contains('modal')) e.target.classList.remove('open');
});

document.addEventListener('keydown', function (e) {
  if (e.key === 'Escape')
    document.querySelectorAll('.modal.open').forEach(function (m) { m.classList.remove('open'); });
});

// ---- quantity "-" / "+" buttons: <button onclick="stepQty(this,1)"> ----
function stepQty(button, delta) {
  var input = button.parentNode.querySelector('input');
  var value = parseInt(input.value || '1', 10) + delta;
  var min = parseInt(input.min || '1', 10);
  var max = parseInt(input.max || '99', 10);
  input.value = Math.max(min, Math.min(max, value));
}

// ---- hide the green/red message bar after a few seconds ----
window.addEventListener('load', function () {
  var flash = document.querySelector('.flash');
  if (flash) setTimeout(function () { flash.style.display = 'none'; }, 5000);
});

// ---- delivery countdown: <b class="countdown" data-seconds="75"> shows 1:15, 1:14 ... ----
// When it reaches 0 the page reloads once the server has marked the order delivered.
(function () {
  var counters = document.querySelectorAll('.countdown');
  if (!counters.length) return;

  function show(el, s) {
    var m = Math.floor(s / 60), r = s % 60;
    el.textContent = m + ':' + (r < 10 ? '0' : '') + r;
  }

  counters.forEach(function (el) {
    var left = parseInt(el.dataset.seconds || '0', 10);
    show(el, left);

    var timer = setInterval(function () {
      left--;
      if (left > 0) { show(el, left); return; }
      clearInterval(timer);
      el.textContent = 'delivering...';
      setTimeout(function () { location.reload(); }, 6000);   // the server checks every few seconds
    }, 1000);
  });
})();
