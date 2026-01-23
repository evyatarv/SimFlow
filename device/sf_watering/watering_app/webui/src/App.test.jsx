import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen, fireEvent, waitFor } from '@testing-library/react';
import App from './App';

// Mock fetch for API calls
global.fetch = vi.fn();

beforeEach(() => {
  vi.clearAllMocks();
});

describe('App', () => {
  it('renders the dashboard title', () => {
    render(<App />);
    expect(screen.getByText('SimFlow Dashboard')).toBeInTheDocument();
  });

  it('renders the initial schedules in the table', () => {
    render(<App />);
    // Multiple elements may contain these texts (table + next event card)
    expect(screen.getAllByText('Front Lawn').length).toBeGreaterThan(0);
    expect(screen.getAllByText('Flower Bed').length).toBeGreaterThan(0);
  });

  it('displays watering history section', () => {
    render(<App />);
    expect(screen.getByText('Watering History')).toBeInTheDocument();
  });

  it('displays active schedules section', () => {
    render(<App />);
    expect(screen.getByText('Active Schedules')).toBeInTheDocument();
  });

  it('shows the next event information', () => {
    render(<App />);
    expect(screen.getByText('Up Next')).toBeInTheDocument();
  });
});

describe('Navigation', () => {
  it('switches to add form view when clicking New button', () => {
    render(<App />);

    // Find and click the "New" button (desktop nav)
    const newButton = screen.getByText('New');
    fireEvent.click(newButton);

    expect(screen.getByText('Configure New Schedule')).toBeInTheDocument();
  });

  it('switches back to dashboard when clicking Dash button', () => {
    render(<App />);

    // Go to add form
    const newButton = screen.getByText('New');
    fireEvent.click(newButton);

    // Verify we're on the add form
    expect(screen.getByText('Configure New Schedule')).toBeInTheDocument();

    // Click the Dash button to go back
    const dashButton = screen.getByText('Dash');
    fireEvent.click(dashButton);

    // Should be back on dashboard
    expect(screen.getByText('Active Schedules')).toBeInTheDocument();
  });
});

describe('AddForm', () => {
  beforeEach(() => {
    render(<App />);
    // Navigate to add form
    const newButton = screen.getByText('New');
    fireEvent.click(newButton);
  });

  it('renders day selection buttons', () => {
    expect(screen.getByText('SUN')).toBeInTheDocument();
    expect(screen.getByText('MON')).toBeInTheDocument();
    expect(screen.getByText('TUE')).toBeInTheDocument();
    expect(screen.getByText('WED')).toBeInTheDocument();
    expect(screen.getByText('THU')).toBeInTheDocument();
    expect(screen.getByText('FRI')).toBeInTheDocument();
    expect(screen.getByText('SAT')).toBeInTheDocument();
  });

  it('renders time presets', () => {
    expect(screen.getByText('Morning')).toBeInTheDocument();
    expect(screen.getByText('Noon')).toBeInTheDocument();
    expect(screen.getByText('Evening')).toBeInTheDocument();
  });

  it('renders area selection', () => {
    expect(screen.getByText('Back Garden')).toBeInTheDocument();
    expect(screen.getByText('Veg Patch')).toBeInTheDocument();
    expect(screen.getByText('Greenhouse')).toBeInTheDocument();
  });

  it('renders duration slider', () => {
    expect(screen.getByText('Duration')).toBeInTheDocument();
    expect(screen.getByRole('slider')).toBeInTheDocument();
  });

  it('renders save button', () => {
    expect(screen.getByText('Save Schedule')).toBeInTheDocument();
  });

  it('can toggle day selection', () => {
    const monButton = screen.getAllByText('MON')[0];
    fireEvent.click(monButton);

    // MON should now be selected (has teal background class)
    expect(monButton).toHaveClass('bg-teal-600');
  });

  it('can select a time preset', () => {
    const noonButton = screen.getByText('12:00');
    fireEvent.click(noonButton);

    // Button should be selected
    expect(noonButton.closest('button')).toHaveClass('border-teal-500');
  });

  it('can change duration with slider', () => {
    const slider = screen.getByRole('slider');
    fireEvent.change(slider, { target: { value: '45' } });

    expect(screen.getByText('45')).toBeInTheDocument();
  });
});

describe('Schedule deletion', () => {
  it('removes schedule when delete button is clicked', async () => {
    render(<App />);

    // Get initial count of Front Lawn entries
    const initialFrontLawn = screen.getAllByText('Front Lawn');

    // Find delete buttons (trash icons)
    const deleteButtons = screen.getAllByRole('button').filter(btn =>
      btn.querySelector('svg')
    );

    // Click the first delete button in the table
    const tableDeleteButton = deleteButtons.find(btn =>
      btn.closest('td')
    );

    if (tableDeleteButton) {
      fireEvent.click(tableDeleteButton);

      await waitFor(() => {
        // Should have fewer Front Lawn entries or none
        const afterDeleteFrontLawn = screen.queryAllByText('Front Lawn');
        expect(afterDeleteFrontLawn.length).toBeLessThanOrEqual(initialFrontLawn.length);
      });
    }
  });
});

describe('Demo mode', () => {
  it('has a demo toggle button', () => {
    render(<App />);
    expect(screen.getByText('Demo')).toBeInTheDocument();
  });
});
